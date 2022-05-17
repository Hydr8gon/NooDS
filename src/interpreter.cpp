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

// ARM lookup table, based on the map found at http://imrannazar.com/ARM-Opcode-Map
// Uses bits 27-20 and 7-4 of an opcode to find the appropriate instruction
int (Interpreter::*Interpreter::armInstrs[])(uint32_t) =
{
    &Interpreter::andLli,     &Interpreter::andLlr,     &Interpreter::andLri,     &Interpreter::andLrr,     // 0x000-0x003
    &Interpreter::andAri,     &Interpreter::andArr,     &Interpreter::andRri,     &Interpreter::andRrr,     // 0x004-0x007
    &Interpreter::andLli,     &Interpreter::mul,        &Interpreter::andLri,     &Interpreter::strhPtrm,   // 0x008-0x00B
    &Interpreter::andAri,     &Interpreter::ldrdPtrm,   &Interpreter::andRri,     &Interpreter::strdPtrm,   // 0x00C-0x00F
    &Interpreter::andsLli,    &Interpreter::andsLlr,    &Interpreter::andsLri,    &Interpreter::andsLrr,    // 0x010-0x013
    &Interpreter::andsAri,    &Interpreter::andsArr,    &Interpreter::andsRri,    &Interpreter::andsRrr,    // 0x014-0x017
    &Interpreter::andsLli,    &Interpreter::muls,       &Interpreter::andsLri,    &Interpreter::ldrhPtrm,   // 0x018-0x01B
    &Interpreter::andsAri,    &Interpreter::ldrsbPtrm,  &Interpreter::andsRri,    &Interpreter::ldrshPtrm,  // 0x01C-0x01F
    &Interpreter::eorLli,     &Interpreter::eorLlr,     &Interpreter::eorLri,     &Interpreter::eorLrr,     // 0x020-0x023
    &Interpreter::eorAri,     &Interpreter::eorArr,     &Interpreter::eorRri,     &Interpreter::eorRrr,     // 0x024-0x027
    &Interpreter::eorLli,     &Interpreter::mla,        &Interpreter::eorLri,     &Interpreter::strhPtrm,   // 0x028-0x02B
    &Interpreter::eorAri,     &Interpreter::ldrdPtrm,   &Interpreter::eorRri,     &Interpreter::strdPtrm,   // 0x02C-0x02F
    &Interpreter::eorsLli,    &Interpreter::eorsLlr,    &Interpreter::eorsLri,    &Interpreter::eorsLrr,    // 0x030-0x033
    &Interpreter::eorsAri,    &Interpreter::eorsArr,    &Interpreter::eorsRri,    &Interpreter::eorsRrr,    // 0x034-0x037
    &Interpreter::eorsLli,    &Interpreter::mlas,       &Interpreter::eorsLri,    &Interpreter::ldrhPtrm,   // 0x038-0x03B
    &Interpreter::eorsAri,    &Interpreter::ldrsbPtrm,  &Interpreter::eorsRri,    &Interpreter::ldrshPtrm,  // 0x03C-0x03F
    &Interpreter::subLli,     &Interpreter::subLlr,     &Interpreter::subLri,     &Interpreter::subLrr,     // 0x040-0x043
    &Interpreter::subAri,     &Interpreter::subArr,     &Interpreter::subRri,     &Interpreter::subRrr,     // 0x044-0x047
    &Interpreter::subLli,     &Interpreter::unkArm,     &Interpreter::subLri,     &Interpreter::strhPtim,   // 0x048-0x04B
    &Interpreter::subAri,     &Interpreter::ldrdPtim,   &Interpreter::subRri,     &Interpreter::strdPtim,   // 0x04C-0x04F
    &Interpreter::subsLli,    &Interpreter::subsLlr,    &Interpreter::subsLri,    &Interpreter::subsLrr,    // 0x050-0x053
    &Interpreter::subsAri,    &Interpreter::subsArr,    &Interpreter::subsRri,    &Interpreter::subsRrr,    // 0x054-0x057
    &Interpreter::subsLli,    &Interpreter::unkArm,     &Interpreter::subsLri,    &Interpreter::ldrhPtim,   // 0x058-0x05B
    &Interpreter::subsAri,    &Interpreter::ldrsbPtim,  &Interpreter::subsRri,    &Interpreter::ldrshPtim,  // 0x05C-0x05F
    &Interpreter::rsbLli,     &Interpreter::rsbLlr,     &Interpreter::rsbLri,     &Interpreter::rsbLrr,     // 0x060-0x063
    &Interpreter::rsbAri,     &Interpreter::rsbArr,     &Interpreter::rsbRri,     &Interpreter::rsbRrr,     // 0x064-0x067
    &Interpreter::rsbLli,     &Interpreter::unkArm,     &Interpreter::rsbLri,     &Interpreter::strhPtim,   // 0x068-0x06B
    &Interpreter::rsbAri,     &Interpreter::ldrdPtim,   &Interpreter::rsbRri,     &Interpreter::strdPtim,   // 0x06C-0x06F
    &Interpreter::rsbsLli,    &Interpreter::rsbsLlr,    &Interpreter::rsbsLri,    &Interpreter::rsbsLrr,    // 0x070-0x073
    &Interpreter::rsbsAri,    &Interpreter::rsbsArr,    &Interpreter::rsbsRri,    &Interpreter::rsbsRrr,    // 0x074-0x077
    &Interpreter::rsbsLli,    &Interpreter::unkArm,     &Interpreter::rsbsLri,    &Interpreter::ldrhPtim,   // 0x078-0x07B
    &Interpreter::rsbsAri,    &Interpreter::ldrsbPtim,  &Interpreter::rsbsRri,    &Interpreter::ldrshPtim,  // 0x07C-0x07F
    &Interpreter::addLli,     &Interpreter::addLlr,     &Interpreter::addLri,     &Interpreter::addLrr,     // 0x080-0x083
    &Interpreter::addAri,     &Interpreter::addArr,     &Interpreter::addRri,     &Interpreter::addRrr,     // 0x084-0x087
    &Interpreter::addLli,     &Interpreter::umull,      &Interpreter::addLri,     &Interpreter::strhPtrp,   // 0x088-0x08B
    &Interpreter::addAri,     &Interpreter::ldrdPtrp,   &Interpreter::addRri,     &Interpreter::strdPtrp,   // 0x08C-0x08F
    &Interpreter::addsLli,    &Interpreter::addsLlr,    &Interpreter::addsLri,    &Interpreter::addsLrr,    // 0x090-0x093
    &Interpreter::addsAri,    &Interpreter::addsArr,    &Interpreter::addsRri,    &Interpreter::addsRrr,    // 0x094-0x097
    &Interpreter::addsLli,    &Interpreter::umulls,     &Interpreter::addsLri,    &Interpreter::ldrhPtrp,   // 0x098-0x09B
    &Interpreter::addsAri,    &Interpreter::ldrsbPtrp,  &Interpreter::addsRri,    &Interpreter::ldrshPtrp,  // 0x09C-0x09F
    &Interpreter::adcLli,     &Interpreter::adcLlr,     &Interpreter::adcLri,     &Interpreter::adcLrr,     // 0x0A0-0x0A3
    &Interpreter::adcAri,     &Interpreter::adcArr,     &Interpreter::adcRri,     &Interpreter::adcRrr,     // 0x0A4-0x0A7
    &Interpreter::adcLli,     &Interpreter::umlal,      &Interpreter::adcLri,     &Interpreter::strhPtrp,   // 0x0A8-0x0AB
    &Interpreter::adcAri,     &Interpreter::ldrdPtrp,   &Interpreter::adcRri,     &Interpreter::strdPtrp,   // 0x0AC-0x0AF
    &Interpreter::adcsLli,    &Interpreter::adcsLlr,    &Interpreter::adcsLri,    &Interpreter::adcsLrr,    // 0x0B0-0x0B3
    &Interpreter::adcsAri,    &Interpreter::adcsArr,    &Interpreter::adcsRri,    &Interpreter::adcsRrr,    // 0x0B4-0x0B7
    &Interpreter::adcsLli,    &Interpreter::umlals,     &Interpreter::adcsLri,    &Interpreter::ldrhPtrp,   // 0x0B8-0x0BB
    &Interpreter::adcsAri,    &Interpreter::ldrsbPtrp,  &Interpreter::adcsRri,    &Interpreter::ldrshPtrp,  // 0x0BC-0x0BF
    &Interpreter::sbcLli,     &Interpreter::sbcLlr,     &Interpreter::sbcLri,     &Interpreter::sbcLrr,     // 0x0C0-0x0C3
    &Interpreter::sbcAri,     &Interpreter::sbcArr,     &Interpreter::sbcRri,     &Interpreter::sbcRrr,     // 0x0C4-0x0C7
    &Interpreter::sbcLli,     &Interpreter::smull,      &Interpreter::sbcLri,     &Interpreter::strhPtip,   // 0x0C8-0x0CB
    &Interpreter::sbcAri,     &Interpreter::ldrdPtip,   &Interpreter::sbcRri,     &Interpreter::strdPtip,   // 0x0CC-0x0CF
    &Interpreter::sbcsLli,    &Interpreter::sbcsLlr,    &Interpreter::sbcsLri,    &Interpreter::sbcsLrr,    // 0x0D0-0x0D3
    &Interpreter::sbcsAri,    &Interpreter::sbcsArr,    &Interpreter::sbcsRri,    &Interpreter::sbcsRrr,    // 0x0D4-0x0D7
    &Interpreter::sbcsLli,    &Interpreter::smulls,     &Interpreter::sbcsLri,    &Interpreter::ldrhPtip,   // 0x0D8-0x0DB
    &Interpreter::sbcsAri,    &Interpreter::ldrsbPtip,  &Interpreter::sbcsRri,    &Interpreter::ldrshPtip,  // 0x0DC-0x0DF
    &Interpreter::rscLli,     &Interpreter::rscLlr,     &Interpreter::rscLri,     &Interpreter::rscLrr,     // 0x0E0-0x0E3
    &Interpreter::rscAri,     &Interpreter::rscArr,     &Interpreter::rscRri,     &Interpreter::rscRrr,     // 0x0E4-0x0E7
    &Interpreter::rscLli,     &Interpreter::smlal,      &Interpreter::rscLri,     &Interpreter::strhPtip,   // 0x0E8-0x0EB
    &Interpreter::rscAri,     &Interpreter::ldrdPtip,   &Interpreter::rscRri,     &Interpreter::strdPtip,   // 0x0EC-0x0EF
    &Interpreter::rscsLli,    &Interpreter::rscsLlr,    &Interpreter::rscsLri,    &Interpreter::rscsLrr,    // 0x0F0-0x0F3
    &Interpreter::rscsAri,    &Interpreter::rscsArr,    &Interpreter::rscsRri,    &Interpreter::rscsRrr,    // 0x0F4-0x0F7
    &Interpreter::rscsLli,    &Interpreter::smlals,     &Interpreter::rscsLri,    &Interpreter::ldrhPtip,   // 0x0F8-0x0FB
    &Interpreter::rscsAri,    &Interpreter::ldrsbPtip,  &Interpreter::rscsRri,    &Interpreter::ldrshPtip,  // 0x0FC-0x0FF
    &Interpreter::mrsRc,      &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x100-0x103
    &Interpreter::unkArm,     &Interpreter::qadd,       &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x104-0x107
    &Interpreter::smlabb,     &Interpreter::swp,        &Interpreter::smlatb,     &Interpreter::strhOfrm,   // 0x108-0x10B
    &Interpreter::smlabt,     &Interpreter::ldrdOfrm,   &Interpreter::smlatt,     &Interpreter::strdOfrm,   // 0x10C-0x10F
    &Interpreter::tstLli,     &Interpreter::tstLlr,     &Interpreter::tstLri,     &Interpreter::tstLrr,     // 0x110-0x113
    &Interpreter::tstAri,     &Interpreter::tstArr,     &Interpreter::tstRri,     &Interpreter::tstRrr,     // 0x114-0x117
    &Interpreter::tstLli,     &Interpreter::unkArm,     &Interpreter::tstLri,     &Interpreter::ldrhOfrm,   // 0x118-0x11B
    &Interpreter::tstAri,     &Interpreter::ldrsbOfrm,  &Interpreter::tstRri,     &Interpreter::ldrshOfrm,  // 0x11C-0x11F
    &Interpreter::msrRc,      &Interpreter::bx,         &Interpreter::unkArm,     &Interpreter::blxReg,     // 0x120-0x123
    &Interpreter::unkArm,     &Interpreter::qsub,       &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x124-0x127
    &Interpreter::smlawb,     &Interpreter::unkArm,     &Interpreter::smulwb,     &Interpreter::strhPrrm,   // 0x128-0x12B
    &Interpreter::smlawt,     &Interpreter::ldrdPrrm,   &Interpreter::smulwt,     &Interpreter::strdPrrm,   // 0x12C-0x12F
    &Interpreter::teqLli,     &Interpreter::teqLlr,     &Interpreter::teqLri,     &Interpreter::teqLrr,     // 0x130-0x133
    &Interpreter::teqAri,     &Interpreter::teqArr,     &Interpreter::teqRri,     &Interpreter::teqRrr,     // 0x134-0x137
    &Interpreter::teqLli,     &Interpreter::unkArm,     &Interpreter::teqLri,     &Interpreter::ldrhPrrm,   // 0x138-0x13B
    &Interpreter::teqAri,     &Interpreter::ldrsbPrrm,  &Interpreter::teqRri,     &Interpreter::ldrshPrrm,  // 0x13C-0x13F
    &Interpreter::mrsRs,      &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x140-0x143
    &Interpreter::unkArm,     &Interpreter::qdadd,      &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x144-0x147
    &Interpreter::smlalbb,    &Interpreter::swpb,       &Interpreter::smlaltb,    &Interpreter::strhOfim,   // 0x148-0x14B
    &Interpreter::smlalbt,    &Interpreter::ldrdOfim,   &Interpreter::smlaltt,    &Interpreter::strdOfim,   // 0x14C-0x14F
    &Interpreter::cmpLli,     &Interpreter::cmpLlr,     &Interpreter::cmpLri,     &Interpreter::cmpLrr,     // 0x150-0x153
    &Interpreter::cmpAri,     &Interpreter::cmpArr,     &Interpreter::cmpRri,     &Interpreter::cmpRrr,     // 0x154-0x157
    &Interpreter::cmpLli,     &Interpreter::unkArm,     &Interpreter::cmpLri,     &Interpreter::ldrhOfim,   // 0x158-0x15B
    &Interpreter::cmpAri,     &Interpreter::ldrsbOfim,  &Interpreter::cmpRri,     &Interpreter::ldrshOfim,  // 0x15C-0x15F
    &Interpreter::msrRs,      &Interpreter::clz,        &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x160-0x163
    &Interpreter::unkArm,     &Interpreter::qdsub,      &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x164-0x167
    &Interpreter::smulbb,     &Interpreter::unkArm,     &Interpreter::smultb,     &Interpreter::strhPrim,   // 0x168-0x16B
    &Interpreter::smulbt,     &Interpreter::ldrdPrim,   &Interpreter::smultt,     &Interpreter::strdPrim,   // 0x16C-0x16F
    &Interpreter::cmnLli,     &Interpreter::cmnLlr,     &Interpreter::cmnLri,     &Interpreter::cmnLrr,     // 0x170-0x173
    &Interpreter::cmnAri,     &Interpreter::cmnArr,     &Interpreter::cmnRri,     &Interpreter::cmnRrr,     // 0x174-0x177
    &Interpreter::cmnLli,     &Interpreter::unkArm,     &Interpreter::cmnLri,     &Interpreter::ldrhPrim,   // 0x178-0x17B
    &Interpreter::cmnAri,     &Interpreter::ldrsbPrim,  &Interpreter::cmnRri,     &Interpreter::ldrshPrim,  // 0x17C-0x17F
    &Interpreter::orrLli,     &Interpreter::orrLlr,     &Interpreter::orrLri,     &Interpreter::orrLrr,     // 0x180-0x183
    &Interpreter::orrAri,     &Interpreter::orrArr,     &Interpreter::orrRri,     &Interpreter::orrRrr,     // 0x184-0x187
    &Interpreter::orrLli,     &Interpreter::unkArm,     &Interpreter::orrLri,     &Interpreter::strhOfrp,   // 0x188-0x18B
    &Interpreter::orrAri,     &Interpreter::ldrdOfrp,   &Interpreter::orrRri,     &Interpreter::strdOfrp,   // 0x18C-0x18F
    &Interpreter::orrsLli,    &Interpreter::orrsLlr,    &Interpreter::orrsLri,    &Interpreter::orrsLrr,    // 0x190-0x193
    &Interpreter::orrsAri,    &Interpreter::orrsArr,    &Interpreter::orrsRri,    &Interpreter::orrsRrr,    // 0x194-0x197
    &Interpreter::orrsLli,    &Interpreter::unkArm,     &Interpreter::orrsLri,    &Interpreter::ldrhOfrp,   // 0x198-0x19B
    &Interpreter::orrsAri,    &Interpreter::ldrsbOfrp,  &Interpreter::orrsRri,    &Interpreter::ldrshOfrp,  // 0x19C-0x19F
    &Interpreter::movLli,     &Interpreter::movLlr,     &Interpreter::movLri,     &Interpreter::movLrr,     // 0x1A0-0x1A3
    &Interpreter::movAri,     &Interpreter::movArr,     &Interpreter::movRri,     &Interpreter::movRrr,     // 0x1A4-0x1A7
    &Interpreter::movLli,     &Interpreter::unkArm,     &Interpreter::movLri,     &Interpreter::strhPrrp,   // 0x1A8-0x1AB
    &Interpreter::movAri,     &Interpreter::ldrdPrrp,   &Interpreter::movRri,     &Interpreter::strdPrrp,   // 0x1AC-0x1AF
    &Interpreter::movsLli,    &Interpreter::movsLlr,    &Interpreter::movsLri,    &Interpreter::movsLrr,    // 0x1B0-0x1B3
    &Interpreter::movsAri,    &Interpreter::movsArr,    &Interpreter::movsRri,    &Interpreter::movsRrr,    // 0x1B4-0x1B7
    &Interpreter::movsLli,    &Interpreter::unkArm,     &Interpreter::movsLri,    &Interpreter::ldrhPrrp,   // 0x1B8-0x1BB
    &Interpreter::movsAri,    &Interpreter::ldrsbPrrp,  &Interpreter::movsRri,    &Interpreter::ldrshPrrp,  // 0x1BC-0x1BF
    &Interpreter::bicLli,     &Interpreter::bicLlr,     &Interpreter::bicLri,     &Interpreter::bicLrr,     // 0x1C0-0x1C3
    &Interpreter::bicAri,     &Interpreter::bicArr,     &Interpreter::bicRri,     &Interpreter::bicRrr,     // 0x1C4-0x1C7
    &Interpreter::bicLli,     &Interpreter::unkArm,     &Interpreter::bicLri,     &Interpreter::strhOfip,   // 0x1C8-0x1CB
    &Interpreter::bicAri,     &Interpreter::ldrdOfip,   &Interpreter::bicRri,     &Interpreter::strdOfip,   // 0x1CC-0x1CF
    &Interpreter::bicsLli,    &Interpreter::bicsLlr,    &Interpreter::bicsLri,    &Interpreter::bicsLrr,    // 0x1D0-0x1D3
    &Interpreter::bicsAri,    &Interpreter::bicsArr,    &Interpreter::bicsRri,    &Interpreter::bicsRrr,    // 0x1D4-0x1D7
    &Interpreter::bicsLli,    &Interpreter::unkArm,     &Interpreter::bicsLri,    &Interpreter::ldrhOfip,   // 0x1D8-0x1DB
    &Interpreter::bicsAri,    &Interpreter::ldrsbOfip,  &Interpreter::bicsRri,    &Interpreter::ldrshOfip,  // 0x1DC-0x1DF
    &Interpreter::mvnLli,     &Interpreter::mvnLlr,     &Interpreter::mvnLri,     &Interpreter::mvnLrr,     // 0x1E0-0x1E3
    &Interpreter::mvnAri,     &Interpreter::mvnArr,     &Interpreter::mvnRri,     &Interpreter::mvnRrr,     // 0x1E4-0x1E7
    &Interpreter::mvnLli,     &Interpreter::unkArm,     &Interpreter::mvnLri,     &Interpreter::strhPrip,   // 0x1E8-0x1EB
    &Interpreter::mvnAri,     &Interpreter::ldrdPrip,   &Interpreter::mvnRri,     &Interpreter::strdPrip,   // 0x1EC-0x1EF
    &Interpreter::mvnsLli,    &Interpreter::mvnsLlr,    &Interpreter::mvnsLri,    &Interpreter::mvnsLrr,    // 0x1F0-0x1F3
    &Interpreter::mvnsAri,    &Interpreter::mvnsArr,    &Interpreter::mvnsRri,    &Interpreter::mvnsRrr,    // 0x1F4-0x1F7
    &Interpreter::mvnsLli,    &Interpreter::unkArm,     &Interpreter::mvnsLri,    &Interpreter::ldrhPrip,   // 0x1F8-0x1FB
    &Interpreter::mvnsAri,    &Interpreter::ldrsbPrip,  &Interpreter::mvnsRri,    &Interpreter::ldrshPrip,  // 0x1FC-0x1FF
    &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     // 0x200-0x203
    &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     // 0x204-0x207
    &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     // 0x208-0x20B
    &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     &Interpreter::andImm,     // 0x20C-0x20F
    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    // 0x210-0x213
    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    // 0x214-0x217
    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    // 0x218-0x21B
    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    &Interpreter::andsImm,    // 0x21C-0x21F
    &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     // 0x220-0x223
    &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     // 0x224-0x227
    &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     // 0x228-0x22B
    &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     &Interpreter::eorImm,     // 0x22C-0x22F
    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    // 0x230-0x233
    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    // 0x234-0x237
    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    // 0x238-0x23B
    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    &Interpreter::eorsImm,    // 0x23C-0x23F
    &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     // 0x240-0x243
    &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     // 0x244-0x247
    &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     // 0x248-0x24B
    &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     &Interpreter::subImm,     // 0x24C-0x24F
    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    // 0x250-0x253
    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    // 0x254-0x257
    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    // 0x258-0x25B
    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    &Interpreter::subsImm,    // 0x25C-0x25F
    &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     // 0x260-0x263
    &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     // 0x264-0x267
    &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     // 0x268-0x26B
    &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     &Interpreter::rsbImm,     // 0x26C-0x26F
    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    // 0x270-0x273
    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    // 0x274-0x277
    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    // 0x278-0x27B
    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    &Interpreter::rsbsImm,    // 0x27C-0x27F
    &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     // 0x280-0x283
    &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     // 0x284-0x287
    &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     // 0x288-0x28B
    &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     &Interpreter::addImm,     // 0x28C-0x28F
    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    // 0x290-0x293
    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    // 0x294-0x297
    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    // 0x298-0x29B
    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    &Interpreter::addsImm,    // 0x29C-0x29F
    &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     // 0x2A0-0x2A3
    &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     // 0x2A4-0x2A7
    &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     // 0x2A8-0x2AB
    &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     &Interpreter::adcImm,     // 0x2AC-0x2AF
    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    // 0x2B0-0x2B3
    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    // 0x2B4-0x2B7
    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    // 0x2B8-0x2BB
    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    &Interpreter::adcsImm,    // 0x2BC-0x2BF
    &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     // 0x2C0-0x2C3
    &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     // 0x2C4-0x2C7
    &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     // 0x2C8-0x2CB
    &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     &Interpreter::sbcImm,     // 0x2CC-0x2CF
    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    // 0x2D0-0x2D3
    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    // 0x2D4-0x2D7
    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    // 0x2D8-0x2DB
    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    &Interpreter::sbcsImm,    // 0x2DC-0x2DF
    &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     // 0x2E0-0x2E3
    &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     // 0x2E4-0x2E7
    &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     // 0x2E8-0x2EB
    &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     &Interpreter::rscImm,     // 0x2EC-0x2EF
    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    // 0x2F0-0x2F3
    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    // 0x2F4-0x2F7
    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    // 0x2F8-0x2FB
    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    &Interpreter::rscsImm,    // 0x2FC-0x2FF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x300-0x303
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x304-0x307
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x308-0x30B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x30C-0x30F
    &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     // 0x310-0x313
    &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     // 0x314-0x317
    &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     // 0x318-0x31B
    &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     &Interpreter::tstImm,     // 0x31C-0x31F
    &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      // 0x320-0x323
    &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      // 0x324-0x327
    &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      // 0x328-0x32B
    &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      &Interpreter::msrIc,      // 0x32C-0x32F
    &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     // 0x330-0x333
    &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     // 0x334-0x337
    &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     // 0x338-0x33B
    &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     &Interpreter::teqImm,     // 0x33C-0x33F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x340-0x343
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x344-0x347
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x348-0x34B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x34C-0x34F
    &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     // 0x350-0x353
    &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     // 0x354-0x357
    &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     // 0x358-0x35B
    &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     &Interpreter::cmpImm,     // 0x35C-0x35F
    &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      // 0x360-0x363
    &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      // 0x364-0x367
    &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      // 0x368-0x36B
    &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      &Interpreter::msrIs,      // 0x36C-0x36F
    &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     // 0x370-0x373
    &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     // 0x374-0x377
    &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     // 0x378-0x37B
    &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     &Interpreter::cmnImm,     // 0x37C-0x37F
    &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     // 0x380-0x383
    &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     // 0x384-0x387
    &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     // 0x388-0x38B
    &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     &Interpreter::orrImm,     // 0x38C-0x38F
    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    // 0x390-0x393
    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    // 0x394-0x397
    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    // 0x398-0x39B
    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    &Interpreter::orrsImm,    // 0x39C-0x39F
    &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     // 0x3A0-0x3A3
    &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     // 0x3A4-0x3A7
    &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     // 0x3A8-0x3AB
    &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     &Interpreter::movImm,     // 0x3AC-0x3AF
    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    // 0x3B0-0x3B3
    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    // 0x3B4-0x3B7
    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    // 0x3B8-0x3BB
    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    &Interpreter::movsImm,    // 0x3BC-0x3BF
    &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     // 0x3C0-0x3C3
    &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     // 0x3C4-0x3C7
    &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     // 0x3C8-0x3CB
    &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     &Interpreter::bicImm,     // 0x3CC-0x3CF
    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    // 0x3D0-0x3D3
    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    // 0x3D4-0x3D7
    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    // 0x3D8-0x3DB
    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    &Interpreter::bicsImm,    // 0x3DC-0x3DF
    &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     // 0x3E0-0x3E3
    &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     // 0x3E4-0x3E7
    &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     // 0x3E8-0x3EB
    &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     &Interpreter::mvnImm,     // 0x3EC-0x3EF
    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    // 0x3F0-0x3F3
    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    // 0x3F4-0x3F7
    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    // 0x3F8-0x3FB
    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    &Interpreter::mvnsImm,    // 0x3FC-0x3FF
    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    // 0x400-0x403
    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    // 0x404-0x407
    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    // 0x408-0x40B
    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    &Interpreter::strPtim,    // 0x40C-0x40F
    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    // 0x410-0x413
    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    // 0x414-0x417
    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    // 0x418-0x41B
    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    &Interpreter::ldrPtim,    // 0x41C-0x41F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x420-0x423
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x424-0x427
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x428-0x42B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x42C-0x42F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x430-0x433
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x434-0x437
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x438-0x43B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x43C-0x43F
    &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   // 0x440-0x443
    &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   // 0x444-0x447
    &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   // 0x448-0x44B
    &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   &Interpreter::strbPtim,   // 0x44C-0x44F
    &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   // 0x450-0x453
    &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   // 0x454-0x457
    &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   // 0x458-0x45B
    &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   &Interpreter::ldrbPtim,   // 0x45C-0x45F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x460-0x463
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x464-0x467
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x468-0x46B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x46C-0x46F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x470-0x473
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x474-0x477
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x478-0x47B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x47C-0x47F
    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    // 0x480-0x483
    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    // 0x484-0x487
    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    // 0x488-0x48B
    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    &Interpreter::strPtip,    // 0x48C-0x48F
    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    // 0x490-0x493
    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    // 0x494-0x497
    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    // 0x498-0x49B
    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    &Interpreter::ldrPtip,    // 0x49C-0x49F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4A0-0x4A3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4A4-0x4A7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4A8-0x4AB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4AC-0x4AF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4B0-0x4B3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4B4-0x4B7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4B8-0x4BB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4BC-0x4BF
    &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   // 0x4C0-0x4C3
    &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   // 0x4C4-0x4C7
    &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   // 0x4C8-0x4CB
    &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   &Interpreter::strbPtip,   // 0x4CC-0x4CF
    &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   // 0x4D0-0x4D3
    &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   // 0x4D4-0x4D7
    &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   // 0x4D8-0x4DB
    &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   &Interpreter::ldrbPtip,   // 0x4DC-0x4DF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4E0-0x4E3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4E4-0x4E7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4E8-0x4EB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4EC-0x4EF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4F0-0x4F3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4F4-0x4F7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4F8-0x4FB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x4FC-0x4FF
    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    // 0x500-0x503
    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    // 0x504-0x507
    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    // 0x508-0x50B
    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    &Interpreter::strOfim,    // 0x50C-0x50F
    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    // 0x510-0x513
    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    // 0x514-0x517
    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    // 0x518-0x51B
    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    &Interpreter::ldrOfim,    // 0x51C-0x51F
    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    // 0x520-0x523
    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    // 0x524-0x527
    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    // 0x528-0x52B
    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    &Interpreter::strPrim,    // 0x52C-0x52F
    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    // 0x530-0x533
    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    // 0x534-0x537
    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    // 0x538-0x53B
    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    &Interpreter::ldrPrim,    // 0x53C-0x53F
    &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   // 0x540-0x543
    &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   // 0x544-0x547
    &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   // 0x548-0x54B
    &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   &Interpreter::strbOfim,   // 0x54C-0x54F
    &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   // 0x550-0x553
    &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   // 0x554-0x557
    &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   // 0x558-0x55B
    &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   &Interpreter::ldrbOfim,   // 0x55C-0x55F
    &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   // 0x560-0x563
    &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   // 0x564-0x567
    &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   // 0x568-0x56B
    &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   &Interpreter::strbPrim,   // 0x56C-0x56F
    &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   // 0x570-0x573
    &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   // 0x574-0x577
    &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   // 0x578-0x57B
    &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   &Interpreter::ldrbPrim,   // 0x57C-0x57F
    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    // 0x580-0x583
    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    // 0x584-0x587
    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    // 0x588-0x58B
    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    &Interpreter::strOfip,    // 0x58C-0x58F
    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    // 0x590-0x593
    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    // 0x594-0x597
    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    // 0x598-0x59B
    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    &Interpreter::ldrOfip,    // 0x59C-0x59F
    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    // 0x5A0-0x5A3
    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    // 0x5A4-0x5A7
    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    // 0x5A8-0x5AB
    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    &Interpreter::strPrip,    // 0x5AC-0x5AF
    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    // 0x5B0-0x5B3
    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    // 0x5B4-0x5B7
    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    // 0x5B8-0x5BB
    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    &Interpreter::ldrPrip,    // 0x5BC-0x5BF
    &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   // 0x5C0-0x5C3
    &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   // 0x5C4-0x5C7
    &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   // 0x5C8-0x5CB
    &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   &Interpreter::strbOfip,   // 0x5CC-0x5CF
    &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   // 0x5D0-0x5D3
    &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   // 0x5D4-0x5D7
    &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   // 0x5D8-0x5DB
    &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   &Interpreter::ldrbOfip,   // 0x5DC-0x5DF
    &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   // 0x5E0-0x5E3
    &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   // 0x5E4-0x5E7
    &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   // 0x5E8-0x5EB
    &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   &Interpreter::strbPrip,   // 0x5EC-0x5EF
    &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   // 0x5F0-0x5F3
    &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   // 0x5F4-0x5F7
    &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   // 0x5F8-0x5FB
    &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   &Interpreter::ldrbPrip,   // 0x5FC-0x5FF
    &Interpreter::strPtrmll,  &Interpreter::unkArm,     &Interpreter::strPtrmlr,  &Interpreter::unkArm,     // 0x600-0x603
    &Interpreter::strPtrmar,  &Interpreter::unkArm,     &Interpreter::strPtrmrr,  &Interpreter::unkArm,     // 0x604-0x607
    &Interpreter::strPtrmll,  &Interpreter::unkArm,     &Interpreter::strPtrmlr,  &Interpreter::unkArm,     // 0x608-0x60B
    &Interpreter::strPtrmar,  &Interpreter::unkArm,     &Interpreter::strPtrmrr,  &Interpreter::unkArm,     // 0x60C-0x60F
    &Interpreter::ldrPtrmll,  &Interpreter::unkArm,     &Interpreter::ldrPtrmlr,  &Interpreter::unkArm,     // 0x610-0x613
    &Interpreter::ldrPtrmar,  &Interpreter::unkArm,     &Interpreter::ldrPtrmrr,  &Interpreter::unkArm,     // 0x614-0x617
    &Interpreter::ldrPtrmll,  &Interpreter::unkArm,     &Interpreter::ldrPtrmlr,  &Interpreter::unkArm,     // 0x618-0x61B
    &Interpreter::ldrPtrmar,  &Interpreter::unkArm,     &Interpreter::ldrPtrmrr,  &Interpreter::unkArm,     // 0x61C-0x61F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x620-0x623
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x624-0x627
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x628-0x62B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x62C-0x62F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x630-0x633
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x634-0x637
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x638-0x63B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x63C-0x63F
    &Interpreter::strbPtrmll, &Interpreter::unkArm,     &Interpreter::strbPtrmlr, &Interpreter::unkArm,     // 0x640-0x643
    &Interpreter::strbPtrmar, &Interpreter::unkArm,     &Interpreter::strbPtrmrr, &Interpreter::unkArm,     // 0x644-0x647
    &Interpreter::strbPtrmll, &Interpreter::unkArm,     &Interpreter::strbPtrmlr, &Interpreter::unkArm,     // 0x648-0x64B
    &Interpreter::strbPtrmar, &Interpreter::unkArm,     &Interpreter::strbPtrmrr, &Interpreter::unkArm,     // 0x64C-0x64F
    &Interpreter::ldrbPtrmll, &Interpreter::unkArm,     &Interpreter::ldrbPtrmlr, &Interpreter::unkArm,     // 0x650-0x653
    &Interpreter::ldrbPtrmar, &Interpreter::unkArm,     &Interpreter::ldrbPtrmrr, &Interpreter::unkArm,     // 0x654-0x657
    &Interpreter::ldrbPtrmll, &Interpreter::unkArm,     &Interpreter::ldrbPtrmlr, &Interpreter::unkArm,     // 0x658-0x65B
    &Interpreter::ldrbPtrmar, &Interpreter::unkArm,     &Interpreter::ldrbPtrmrr, &Interpreter::unkArm,     // 0x65C-0x65F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x660-0x663
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x664-0x667
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x668-0x66B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x66C-0x66F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x670-0x673
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x674-0x677
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x678-0x67B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x67C-0x67F
    &Interpreter::strPtrpll,  &Interpreter::unkArm,     &Interpreter::strPtrplr,  &Interpreter::unkArm,     // 0x680-0x683
    &Interpreter::strPtrpar,  &Interpreter::unkArm,     &Interpreter::strPtrprr,  &Interpreter::unkArm,     // 0x684-0x687
    &Interpreter::strPtrpll,  &Interpreter::unkArm,     &Interpreter::strPtrplr,  &Interpreter::unkArm,     // 0x688-0x68B
    &Interpreter::strPtrpar,  &Interpreter::unkArm,     &Interpreter::strPtrprr,  &Interpreter::unkArm,     // 0x68C-0x68F
    &Interpreter::ldrPtrpll,  &Interpreter::unkArm,     &Interpreter::ldrPtrplr,  &Interpreter::unkArm,     // 0x690-0x693
    &Interpreter::ldrPtrpar,  &Interpreter::unkArm,     &Interpreter::ldrPtrprr,  &Interpreter::unkArm,     // 0x694-0x697
    &Interpreter::ldrPtrpll,  &Interpreter::unkArm,     &Interpreter::ldrPtrplr,  &Interpreter::unkArm,     // 0x698-0x69B
    &Interpreter::ldrPtrpar,  &Interpreter::unkArm,     &Interpreter::ldrPtrprr,  &Interpreter::unkArm,     // 0x69C-0x69F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6A0-0x6A3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6A4-0x6A7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6A8-0x6AB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6AC-0x6AF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6B0-0x6B3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6B4-0x6B7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6B8-0x6BB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6BC-0x6BF
    &Interpreter::strbPtrpll, &Interpreter::unkArm,     &Interpreter::strbPtrplr, &Interpreter::unkArm,     // 0x6C0-0x6C3
    &Interpreter::strbPtrpar, &Interpreter::unkArm,     &Interpreter::strbPtrprr, &Interpreter::unkArm,     // 0x6C4-0x6C7
    &Interpreter::strbPtrpll, &Interpreter::unkArm,     &Interpreter::strbPtrplr, &Interpreter::unkArm,     // 0x6C8-0x6CB
    &Interpreter::strbPtrpar, &Interpreter::unkArm,     &Interpreter::strbPtrprr, &Interpreter::unkArm,     // 0x6CC-0x6CF
    &Interpreter::ldrbPtrpll, &Interpreter::unkArm,     &Interpreter::ldrbPtrplr, &Interpreter::unkArm,     // 0x6D0-0x6D3
    &Interpreter::ldrbPtrpar, &Interpreter::unkArm,     &Interpreter::ldrbPtrprr, &Interpreter::unkArm,     // 0x6D4-0x6D7
    &Interpreter::ldrbPtrpll, &Interpreter::unkArm,     &Interpreter::ldrbPtrplr, &Interpreter::unkArm,     // 0x6D8-0x6DB
    &Interpreter::ldrbPtrpar, &Interpreter::unkArm,     &Interpreter::ldrbPtrprr, &Interpreter::unkArm,     // 0x6DC-0x6DF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6E0-0x6E3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6E4-0x6E7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6E8-0x6EB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6EC-0x6EF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6F0-0x6F3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6F4-0x6F7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6F8-0x6FB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0x6FC-0x6FF
    &Interpreter::strOfrmll,  &Interpreter::unkArm,     &Interpreter::strOfrmlr,  &Interpreter::unkArm,     // 0x700-0x703
    &Interpreter::strOfrmar,  &Interpreter::unkArm,     &Interpreter::strOfrmrr,  &Interpreter::unkArm,     // 0x704-0x707
    &Interpreter::strOfrmll,  &Interpreter::unkArm,     &Interpreter::strOfrmlr,  &Interpreter::unkArm,     // 0x708-0x70B
    &Interpreter::strOfrmar,  &Interpreter::unkArm,     &Interpreter::strOfrmrr,  &Interpreter::unkArm,     // 0x70C-0x70F
    &Interpreter::ldrOfrmll,  &Interpreter::unkArm,     &Interpreter::ldrOfrmlr,  &Interpreter::unkArm,     // 0x710-0x713
    &Interpreter::ldrOfrmar,  &Interpreter::unkArm,     &Interpreter::ldrOfrmrr,  &Interpreter::unkArm,     // 0x714-0x717
    &Interpreter::ldrOfrmll,  &Interpreter::unkArm,     &Interpreter::ldrOfrmlr,  &Interpreter::unkArm,     // 0x718-0x71B
    &Interpreter::ldrOfrmar,  &Interpreter::unkArm,     &Interpreter::ldrOfrmrr,  &Interpreter::unkArm,     // 0x71C-0x71F
    &Interpreter::strPrrmll,  &Interpreter::unkArm,     &Interpreter::strPrrmlr,  &Interpreter::unkArm,     // 0x720-0x723
    &Interpreter::strPrrmar,  &Interpreter::unkArm,     &Interpreter::strPrrmrr,  &Interpreter::unkArm,     // 0x724-0x727
    &Interpreter::strPrrmll,  &Interpreter::unkArm,     &Interpreter::strPrrmlr,  &Interpreter::unkArm,     // 0x728-0x72B
    &Interpreter::strPrrmar,  &Interpreter::unkArm,     &Interpreter::strPrrmrr,  &Interpreter::unkArm,     // 0x72C-0x72F
    &Interpreter::ldrPrrmll,  &Interpreter::unkArm,     &Interpreter::ldrPrrmlr,  &Interpreter::unkArm,     // 0x730-0x733
    &Interpreter::ldrPrrmar,  &Interpreter::unkArm,     &Interpreter::ldrPrrmrr,  &Interpreter::unkArm,     // 0x734-0x737
    &Interpreter::ldrPrrmll,  &Interpreter::unkArm,     &Interpreter::ldrPrrmlr,  &Interpreter::unkArm,     // 0x738-0x73B
    &Interpreter::ldrPrrmar,  &Interpreter::unkArm,     &Interpreter::ldrPrrmrr,  &Interpreter::unkArm,     // 0x73C-0x73F
    &Interpreter::strbOfrmll, &Interpreter::unkArm,     &Interpreter::strbOfrmlr, &Interpreter::unkArm,     // 0x740-0x743
    &Interpreter::strbOfrmar, &Interpreter::unkArm,     &Interpreter::strbOfrmrr, &Interpreter::unkArm,     // 0x744-0x747
    &Interpreter::strbOfrmll, &Interpreter::unkArm,     &Interpreter::strbOfrmlr, &Interpreter::unkArm,     // 0x748-0x74B
    &Interpreter::strbOfrmar, &Interpreter::unkArm,     &Interpreter::strbOfrmrr, &Interpreter::unkArm,     // 0x74C-0x74F
    &Interpreter::ldrbOfrmll, &Interpreter::unkArm,     &Interpreter::ldrbOfrmlr, &Interpreter::unkArm,     // 0x750-0x753
    &Interpreter::ldrbOfrmar, &Interpreter::unkArm,     &Interpreter::ldrbOfrmrr, &Interpreter::unkArm,     // 0x754-0x757
    &Interpreter::ldrbOfrmll, &Interpreter::unkArm,     &Interpreter::ldrbOfrmlr, &Interpreter::unkArm,     // 0x758-0x75B
    &Interpreter::ldrbOfrmar, &Interpreter::unkArm,     &Interpreter::ldrbOfrmrr, &Interpreter::unkArm,     // 0x75C-0x75F
    &Interpreter::strbPrrmll, &Interpreter::unkArm,     &Interpreter::strbPrrmlr, &Interpreter::unkArm,     // 0x760-0x763
    &Interpreter::strbPrrmar, &Interpreter::unkArm,     &Interpreter::strbPrrmrr, &Interpreter::unkArm,     // 0x764-0x767
    &Interpreter::strbPrrmll, &Interpreter::unkArm,     &Interpreter::strbPrrmlr, &Interpreter::unkArm,     // 0x768-0x76B
    &Interpreter::strbPrrmar, &Interpreter::unkArm,     &Interpreter::strbPrrmrr, &Interpreter::unkArm,     // 0x76C-0x76F
    &Interpreter::ldrbPrrmll, &Interpreter::unkArm,     &Interpreter::ldrbPrrmlr, &Interpreter::unkArm,     // 0x770-0x773
    &Interpreter::ldrbPrrmar, &Interpreter::unkArm,     &Interpreter::ldrbPrrmrr, &Interpreter::unkArm,     // 0x774-0x777
    &Interpreter::ldrbPrrmll, &Interpreter::unkArm,     &Interpreter::ldrbPrrmlr, &Interpreter::unkArm,     // 0x778-0x77B
    &Interpreter::ldrbPrrmar, &Interpreter::unkArm,     &Interpreter::ldrbPrrmrr, &Interpreter::unkArm,     // 0x77C-0x77F
    &Interpreter::strOfrpll,  &Interpreter::unkArm,     &Interpreter::strOfrplr,  &Interpreter::unkArm,     // 0x780-0x783
    &Interpreter::strOfrpar,  &Interpreter::unkArm,     &Interpreter::strOfrprr,  &Interpreter::unkArm,     // 0x784-0x787
    &Interpreter::strOfrpll,  &Interpreter::unkArm,     &Interpreter::strOfrplr,  &Interpreter::unkArm,     // 0x788-0x78B
    &Interpreter::strOfrpar,  &Interpreter::unkArm,     &Interpreter::strOfrprr,  &Interpreter::unkArm,     // 0x78C-0x78F
    &Interpreter::ldrOfrpll,  &Interpreter::unkArm,     &Interpreter::ldrOfrplr,  &Interpreter::unkArm,     // 0x790-0x793
    &Interpreter::ldrOfrpar,  &Interpreter::unkArm,     &Interpreter::ldrOfrprr,  &Interpreter::unkArm,     // 0x794-0x797
    &Interpreter::ldrOfrpll,  &Interpreter::unkArm,     &Interpreter::ldrOfrplr,  &Interpreter::unkArm,     // 0x798-0x79B
    &Interpreter::ldrOfrpar,  &Interpreter::unkArm,     &Interpreter::ldrOfrprr,  &Interpreter::unkArm,     // 0x79C-0x79F
    &Interpreter::strPrrpll,  &Interpreter::unkArm,     &Interpreter::strPrrplr,  &Interpreter::unkArm,     // 0x7A0-0x7A3
    &Interpreter::strPrrpar,  &Interpreter::unkArm,     &Interpreter::strPrrprr,  &Interpreter::unkArm,     // 0x7A4-0x7A7
    &Interpreter::strPrrpll,  &Interpreter::unkArm,     &Interpreter::strPrrplr,  &Interpreter::unkArm,     // 0x7A8-0x7AB
    &Interpreter::strPrrpar,  &Interpreter::unkArm,     &Interpreter::strPrrprr,  &Interpreter::unkArm,     // 0x7AC-0x7AF
    &Interpreter::ldrPrrpll,  &Interpreter::unkArm,     &Interpreter::ldrPrrplr,  &Interpreter::unkArm,     // 0x7B0-0x7B3
    &Interpreter::ldrPrrpar,  &Interpreter::unkArm,     &Interpreter::ldrPrrprr,  &Interpreter::unkArm,     // 0x7B4-0x7B7
    &Interpreter::ldrPrrpll,  &Interpreter::unkArm,     &Interpreter::ldrPrrplr,  &Interpreter::unkArm,     // 0x7B8-0x7BB
    &Interpreter::ldrPrrpar,  &Interpreter::unkArm,     &Interpreter::ldrPrrprr,  &Interpreter::unkArm,     // 0x7BC-0x7BF
    &Interpreter::strbOfrpll, &Interpreter::unkArm,     &Interpreter::strbOfrplr, &Interpreter::unkArm,     // 0x7C0-0x7C3
    &Interpreter::strbOfrpar, &Interpreter::unkArm,     &Interpreter::strbOfrprr, &Interpreter::unkArm,     // 0x7C4-0x7C7
    &Interpreter::strbOfrpll, &Interpreter::unkArm,     &Interpreter::strbOfrplr, &Interpreter::unkArm,     // 0x7C8-0x7CB
    &Interpreter::strbOfrpar, &Interpreter::unkArm,     &Interpreter::strbOfrprr, &Interpreter::unkArm,     // 0x7CC-0x7CF
    &Interpreter::ldrbOfrpll, &Interpreter::unkArm,     &Interpreter::ldrbOfrplr, &Interpreter::unkArm,     // 0x7D0-0x7D3
    &Interpreter::ldrbOfrpar, &Interpreter::unkArm,     &Interpreter::ldrbOfrprr, &Interpreter::unkArm,     // 0x7D4-0x7D7
    &Interpreter::ldrbOfrpll, &Interpreter::unkArm,     &Interpreter::ldrbOfrplr, &Interpreter::unkArm,     // 0x7D8-0x7DB
    &Interpreter::ldrbOfrpar, &Interpreter::unkArm,     &Interpreter::ldrbOfrprr, &Interpreter::unkArm,     // 0x7DC-0x7DF
    &Interpreter::strbPrrpll, &Interpreter::unkArm,     &Interpreter::strbPrrplr, &Interpreter::unkArm,     // 0x7E0-0x7E3
    &Interpreter::strbPrrpar, &Interpreter::unkArm,     &Interpreter::strbPrrprr, &Interpreter::unkArm,     // 0x7E4-0x7E7
    &Interpreter::strbPrrpll, &Interpreter::unkArm,     &Interpreter::strbPrrplr, &Interpreter::unkArm,     // 0x7E8-0x7EB
    &Interpreter::strbPrrpar, &Interpreter::unkArm,     &Interpreter::strbPrrprr, &Interpreter::unkArm,     // 0x7EC-0x7EF
    &Interpreter::ldrbPrrpll, &Interpreter::unkArm,     &Interpreter::ldrbPrrplr, &Interpreter::unkArm,     // 0x7F0-0x7F3
    &Interpreter::ldrbPrrpar, &Interpreter::unkArm,     &Interpreter::ldrbPrrprr, &Interpreter::unkArm,     // 0x7F4-0x7F7
    &Interpreter::ldrbPrrpll, &Interpreter::unkArm,     &Interpreter::ldrbPrrplr, &Interpreter::unkArm,     // 0x7F8-0x7FB
    &Interpreter::ldrbPrrpar, &Interpreter::unkArm,     &Interpreter::ldrbPrrprr, &Interpreter::unkArm,     // 0x7FC-0x7FF
    &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      // 0x800-0x803
    &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      // 0x804-0x807
    &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      // 0x808-0x80B
    &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      &Interpreter::stmda,      // 0x80C-0x80F
    &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      // 0x810-0x813
    &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      // 0x814-0x817
    &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      // 0x818-0x81B
    &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      &Interpreter::ldmda,      // 0x81C-0x81F
    &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     // 0x820-0x823
    &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     // 0x824-0x827
    &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     // 0x828-0x82B
    &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     &Interpreter::stmdaW,     // 0x82C-0x82F
    &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     // 0x830-0x833
    &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     // 0x834-0x837
    &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     // 0x838-0x83B
    &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     &Interpreter::ldmdaW,     // 0x83C-0x83F
    &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     // 0x840-0x843
    &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     // 0x844-0x847
    &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     // 0x848-0x84B
    &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     &Interpreter::stmdaU,     // 0x84C-0x84F
    &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     // 0x850-0x853
    &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     // 0x854-0x857
    &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     // 0x858-0x85B
    &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     &Interpreter::ldmdaU,     // 0x85C-0x85F
    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    // 0x860-0x863
    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    // 0x864-0x867
    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    // 0x868-0x86B
    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    &Interpreter::stmdaUW,    // 0x86C-0x86F
    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    // 0x870-0x873
    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    // 0x874-0x877
    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    // 0x878-0x87B
    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    &Interpreter::ldmdaUW,    // 0x87C-0x87F
    &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      // 0x880-0x883
    &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      // 0x884-0x887
    &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      // 0x888-0x88B
    &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      &Interpreter::stmia,      // 0x88C-0x88F
    &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      // 0x890-0x893
    &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      // 0x894-0x897
    &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      // 0x898-0x89B
    &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      &Interpreter::ldmia,      // 0x89C-0x89F
    &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     // 0x8A0-0x8A3
    &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     // 0x8A4-0x8A7
    &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     // 0x8A8-0x8AB
    &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     &Interpreter::stmiaW,     // 0x8AC-0x8AF
    &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     // 0x8B0-0x8B3
    &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     // 0x8B4-0x8B7
    &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     // 0x8B8-0x8BB
    &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     &Interpreter::ldmiaW,     // 0x8BC-0x8BF
    &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     // 0x8C0-0x8C3
    &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     // 0x8C4-0x8C7
    &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     // 0x8C8-0x8CB
    &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     &Interpreter::stmiaU,     // 0x8CC-0x8CF
    &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     // 0x8D0-0x8D3
    &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     // 0x8D4-0x8D7
    &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     // 0x8D8-0x8DB
    &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     &Interpreter::ldmiaU,     // 0x8DC-0x8DF
    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    // 0x8E0-0x8E3
    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    // 0x8E4-0x8E7
    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    // 0x8E8-0x8EB
    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    &Interpreter::stmiaUW,    // 0x8EC-0x8EF
    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    // 0x8F0-0x8F3
    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    // 0x8F4-0x8F7
    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    // 0x8F8-0x8FB
    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    &Interpreter::ldmiaUW,    // 0x8FC-0x8FF
    &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      // 0x900-0x903
    &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      // 0x904-0x907
    &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      // 0x908-0x90B
    &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      &Interpreter::stmdb,      // 0x90C-0x90F
    &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      // 0x910-0x913
    &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      // 0x914-0x917
    &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      // 0x918-0x91B
    &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      &Interpreter::ldmdb,      // 0x91C-0x91F
    &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     // 0x920-0x923
    &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     // 0x924-0x927
    &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     // 0x928-0x92B
    &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     &Interpreter::stmdbW,     // 0x92C-0x92F
    &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     // 0x930-0x933
    &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     // 0x934-0x937
    &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     // 0x938-0x93B
    &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     &Interpreter::ldmdbW,     // 0x93C-0x93F
    &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     // 0x940-0x943
    &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     // 0x944-0x947
    &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     // 0x948-0x94B
    &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     &Interpreter::stmdbU,     // 0x94C-0x94F
    &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     // 0x950-0x953
    &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     // 0x954-0x957
    &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     // 0x958-0x95B
    &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     &Interpreter::ldmdbU,     // 0x95C-0x95F
    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    // 0x960-0x963
    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    // 0x964-0x967
    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    // 0x968-0x96B
    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    &Interpreter::stmdbUW,    // 0x96C-0x96F
    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    // 0x970-0x973
    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    // 0x974-0x977
    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    // 0x978-0x97B
    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    &Interpreter::ldmdbUW,    // 0x97C-0x97F
    &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      // 0x980-0x983
    &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      // 0x984-0x987
    &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      // 0x988-0x98B
    &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      &Interpreter::stmib,      // 0x98C-0x98F
    &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      // 0x990-0x993
    &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      // 0x994-0x997
    &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      // 0x998-0x99B
    &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      &Interpreter::ldmib,      // 0x99C-0x99F
    &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     // 0x9A0-0x9A3
    &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     // 0x9A4-0x9A7
    &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     // 0x9A8-0x9AB
    &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     &Interpreter::stmibW,     // 0x9AC-0x9AF
    &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     // 0x9B0-0x9B3
    &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     // 0x9B4-0x9B7
    &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     // 0x9B8-0x9BB
    &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     &Interpreter::ldmibW,     // 0x9BC-0x9BF
    &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     // 0x9C0-0x9C3
    &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     // 0x9C4-0x9C7
    &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     // 0x9C8-0x9CB
    &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     &Interpreter::stmibU,     // 0x9CC-0x9CF
    &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     // 0x9D0-0x9D3
    &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     // 0x9D4-0x9D7
    &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     // 0x9D8-0x9DB
    &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     &Interpreter::ldmibU,     // 0x9DC-0x9DF
    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    // 0x9E0-0x9E3
    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    // 0x9E4-0x9E7
    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    // 0x9E8-0x9EB
    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    &Interpreter::stmibUW,    // 0x9EC-0x9EF
    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    // 0x9F0-0x9F3
    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    // 0x9F4-0x9F7
    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    // 0x9F8-0x9FB
    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    &Interpreter::ldmibUW,    // 0x9FC-0x9FF
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA00-0xA03
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA04-0xA07
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA08-0xA0B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA0C-0xA0F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA10-0xA13
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA14-0xA17
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA18-0xA1B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA1C-0xA1F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA20-0xA23
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA24-0xA27
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA28-0xA2B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA2C-0xA2F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA30-0xA33
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA34-0xA37
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA38-0xA3B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA3C-0xA3F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA40-0xA43
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA44-0xA47
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA48-0xA4B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA4C-0xA4F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA50-0xA53
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA54-0xA57
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA58-0xA5B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA5C-0xA5F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA60-0xA63
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA64-0xA67
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA68-0xA6B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA6C-0xA6F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA70-0xA73
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA74-0xA77
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA78-0xA7B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA7C-0xA7F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA80-0xA83
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA84-0xA87
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA88-0xA8B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA8C-0xA8F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA90-0xA93
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA94-0xA97
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA98-0xA9B
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xA9C-0xA9F
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAA0-0xAA3
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAA4-0xAA7
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAA8-0xAAB
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAAC-0xAAF
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAB0-0xAB3
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAB4-0xAB7
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAB8-0xABB
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xABC-0xABF
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAC0-0xAC3
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAC4-0xAC7
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAC8-0xACB
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xACC-0xACF
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAD0-0xAD3
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAD4-0xAD7
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAD8-0xADB
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xADC-0xADF
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAE0-0xAE3
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAE4-0xAE7
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAE8-0xAEB
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAEC-0xAEF
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAF0-0xAF3
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAF4-0xAF7
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAF8-0xAFB
    &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          &Interpreter::b,          // 0xAFC-0xAFF
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB00-0xB03
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB04-0xB07
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB08-0xB0B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB0C-0xB0F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB10-0xB13
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB14-0xB17
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB18-0xB1B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB1C-0xB1F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB20-0xB23
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB24-0xB27
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB28-0xB2B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB2C-0xB2F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB30-0xB33
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB34-0xB37
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB38-0xB3B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB3C-0xB3F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB40-0xB43
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB44-0xB47
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB48-0xB4B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB4C-0xB4F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB50-0xB53
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB54-0xB57
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB58-0xB5B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB5C-0xB5F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB60-0xB63
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB64-0xB67
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB68-0xB6B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB6C-0xB6F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB70-0xB73
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB74-0xB77
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB78-0xB7B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB7C-0xB7F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB80-0xB83
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB84-0xB87
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB88-0xB8B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB8C-0xB8F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB90-0xB93
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB94-0xB97
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB98-0xB9B
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xB9C-0xB9F
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBA0-0xBA3
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBA4-0xBA7
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBA8-0xBAB
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBAC-0xBAF
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBB0-0xBB3
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBB4-0xBB7
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBB8-0xBBB
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBBC-0xBBF
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBC0-0xBC3
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBC4-0xBC7
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBC8-0xBCB
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBCC-0xBCF
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBD0-0xBD3
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBD4-0xBD7
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBD8-0xBDB
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBDC-0xBDF
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBE0-0xBE3
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBE4-0xBE7
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBE8-0xBEB
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBEC-0xBEF
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBF0-0xBF3
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBF4-0xBF7
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBF8-0xBFB
    &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         &Interpreter::bl,         // 0xBFC-0xBFF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC00-0xC03
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC04-0xC07
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC08-0xC0B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC0C-0xC0F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC10-0xC13
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC14-0xC17
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC18-0xC1B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC1C-0xC1F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC20-0xC23
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC24-0xC27
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC28-0xC2B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC2C-0xC2F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC30-0xC33
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC34-0xC37
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC38-0xC3B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC3C-0xC3F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC40-0xC43
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC44-0xC47
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC48-0xC4B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC4C-0xC4F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC50-0xC53
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC54-0xC57
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC58-0xC5B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC5C-0xC5F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC60-0xC63
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC64-0xC67
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC68-0xC6B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC6C-0xC6F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC70-0xC73
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC74-0xC77
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC78-0xC7B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC7C-0xC7F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC80-0xC83
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC84-0xC87
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC88-0xC8B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC8C-0xC8F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC90-0xC93
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC94-0xC97
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC98-0xC9B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xC9C-0xC9F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCA0-0xCA3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCA4-0xCA7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCA8-0xCAB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCAC-0xCAF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCB0-0xCB3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCB4-0xCB7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCB8-0xCBB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCBC-0xCBF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCC0-0xCC3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCC4-0xCC7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCC8-0xCCB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCCC-0xCCF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCD0-0xCD3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCD4-0xCD7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCD8-0xCDB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCDC-0xCDF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCE0-0xCE3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCE4-0xCE7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCE8-0xCEB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCEC-0xCEF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCF0-0xCF3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCF4-0xCF7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCF8-0xCFB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xCFC-0xCFF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD00-0xD03
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD04-0xD07
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD08-0xD0B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD0C-0xD0F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD10-0xD13
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD14-0xD17
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD18-0xD1B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD1C-0xD1F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD20-0xD23
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD24-0xD27
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD28-0xD2B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD2C-0xD2F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD30-0xD33
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD34-0xD37
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD38-0xD3B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD3C-0xD3F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD40-0xD43
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD44-0xD47
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD48-0xD4B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD4C-0xD4F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD50-0xD53
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD54-0xD57
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD58-0xD5B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD5C-0xD5F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD60-0xD63
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD64-0xD67
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD68-0xD6B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD6C-0xD6F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD70-0xD73
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD74-0xD77
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD78-0xD7B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD7C-0xD7F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD80-0xD83
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD84-0xD87
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD88-0xD8B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD8C-0xD8F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD90-0xD93
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD94-0xD97
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD98-0xD9B
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xD9C-0xD9F
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDA0-0xDA3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDA4-0xDA7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDA8-0xDAB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDAC-0xDAF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDB0-0xDB3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDB4-0xDB7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDB8-0xDBB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDBC-0xDBF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDC0-0xDC3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDC4-0xDC7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDC8-0xDCB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDCC-0xDCF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDD0-0xDD3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDD4-0xDD7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDD8-0xDDB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDDC-0xDDF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDE0-0xDE3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDE4-0xDE7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDE8-0xDEB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDEC-0xDEF
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDF0-0xDF3
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDF4-0xDF7
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDF8-0xDFB
    &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     &Interpreter::unkArm,     // 0xDFC-0xDFF
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE00-0xE03
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE04-0xE07
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE08-0xE0B
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE0C-0xE0F
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE10-0xE13
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE14-0xE17
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE18-0xE1B
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE1C-0xE1F
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE20-0xE23
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE24-0xE27
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE28-0xE2B
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE2C-0xE2F
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE30-0xE33
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE34-0xE37
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE38-0xE3B
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE3C-0xE3F
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE40-0xE43
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE44-0xE47
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE48-0xE4B
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE4C-0xE4F
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE50-0xE53
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE54-0xE57
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE58-0xE5B
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE5C-0xE5F
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE60-0xE63
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE64-0xE67
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE68-0xE6B
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE6C-0xE6F
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE70-0xE73
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE74-0xE77
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE78-0xE7B
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE7C-0xE7F
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE80-0xE83
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE84-0xE87
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE88-0xE8B
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xE8C-0xE8F
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE90-0xE93
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE94-0xE97
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE98-0xE9B
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xE9C-0xE9F
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEA0-0xEA3
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEA4-0xEA7
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEA8-0xEAB
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEAC-0xEAF
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEB0-0xEB3
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEB4-0xEB7
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEB8-0xEBB
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEBC-0xEBF
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEC0-0xEC3
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEC4-0xEC7
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEC8-0xECB
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xECC-0xECF
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xED0-0xED3
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xED4-0xED7
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xED8-0xEDB
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEDC-0xEDF
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEE0-0xEE3
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEE4-0xEE7
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEE8-0xEEB
    &Interpreter::unkArm,     &Interpreter::mcr,        &Interpreter::unkArm,     &Interpreter::mcr,        // 0xEEC-0xEEF
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEF0-0xEF3
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEF4-0xEF7
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEF8-0xEFB
    &Interpreter::unkArm,     &Interpreter::mrc,        &Interpreter::unkArm,     &Interpreter::mrc,        // 0xEFC-0xEFF
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF00-0xF03
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF04-0xF07
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF08-0xF0B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF0C-0xF0F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF10-0xF13
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF14-0xF17
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF18-0xF1B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF1C-0xF1F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF20-0xF23
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF24-0xF27
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF28-0xF2B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF2C-0xF2F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF30-0xF33
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF34-0xF37
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF38-0xF3B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF3C-0xF3F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF40-0xF43
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF44-0xF47
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF48-0xF4B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF4C-0xF4F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF50-0xF53
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF54-0xF57
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF58-0xF5B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF5C-0xF5F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF60-0xF63
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF64-0xF67
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF68-0xF6B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF6C-0xF6F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF70-0xF73
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF74-0xF77
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF78-0xF7B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF7C-0xF7F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF80-0xF83
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF84-0xF87
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF88-0xF8B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF8C-0xF8F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF90-0xF93
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF94-0xF97
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF98-0xF9B
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xF9C-0xF9F
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFA0-0xFA3
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFA4-0xFA7
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFA8-0xFAB
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFAC-0xFAF
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFB0-0xFB3
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFB4-0xFB7
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFB8-0xFBB
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFBC-0xFBF
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFC0-0xFC3
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFC4-0xFC7
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFC8-0xFCB
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFCC-0xFCF
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFD0-0xFD3
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFD4-0xFD7
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFD8-0xFDB
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFDC-0xFDF
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFE0-0xFE3
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFE4-0xFE7
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFE8-0xFEB
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFEC-0xFEF
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFF0-0xFF3
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFF4-0xFF7
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        // 0xFF8-0xFFB
    &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi,        &Interpreter::swi         // 0xFFC-0xFFF
};

// THUMB lookup table, based on the map found at http://imrannazar.com/ARM-Opcode-Map
// Uses bits 15-6 of an opcode to find the appropriate instruction
int (Interpreter::*Interpreter::thumbInstrs[])(uint16_t) =
{
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x000-0x003
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x004-0x007
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x008-0x00B
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x00C-0x00F
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x010-0x013
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x014-0x017
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x018-0x01B
    &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   &Interpreter::lslImmT,   // 0x01C-0x01F
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x020-0x023
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x024-0x027
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x028-0x02B
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x02C-0x02F
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x030-0x033
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x034-0x037
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x038-0x03B
    &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   &Interpreter::lsrImmT,   // 0x03C-0x03F
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x040-0x043
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x044-0x047
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x048-0x04B
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x04C-0x04F
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x050-0x053
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x054-0x057
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x058-0x05B
    &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   &Interpreter::asrImmT,   // 0x05C-0x05F
    &Interpreter::addRegT,   &Interpreter::addRegT,   &Interpreter::addRegT,   &Interpreter::addRegT,   // 0x060-0x063
    &Interpreter::addRegT,   &Interpreter::addRegT,   &Interpreter::addRegT,   &Interpreter::addRegT,   // 0x064-0x067
    &Interpreter::subRegT,   &Interpreter::subRegT,   &Interpreter::subRegT,   &Interpreter::subRegT,   // 0x068-0x06B
    &Interpreter::subRegT,   &Interpreter::subRegT,   &Interpreter::subRegT,   &Interpreter::subRegT,   // 0x06C-0x06F
    &Interpreter::addImm3T,  &Interpreter::addImm3T,  &Interpreter::addImm3T,  &Interpreter::addImm3T,  // 0x070-0x073
    &Interpreter::addImm3T,  &Interpreter::addImm3T,  &Interpreter::addImm3T,  &Interpreter::addImm3T,  // 0x074-0x077
    &Interpreter::subImm3T,  &Interpreter::subImm3T,  &Interpreter::subImm3T,  &Interpreter::subImm3T,  // 0x078-0x07B
    &Interpreter::subImm3T,  &Interpreter::subImm3T,  &Interpreter::subImm3T,  &Interpreter::subImm3T,  // 0x07C-0x07F
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x080-0x083
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x084-0x087
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x088-0x08B
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x08C-0x08F
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x090-0x093
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x094-0x097
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x098-0x09B
    &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  &Interpreter::movImm8T,  // 0x09C-0x09F
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0A0-0x0A3
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0A4-0x0A7
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0A8-0x0AB
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0AC-0x0AF
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0B0-0x0B3
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0B4-0x0B7
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0B8-0x0BB
    &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  &Interpreter::cmpImm8T,  // 0x0BC-0x0BF
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0C0-0x0C3
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0C4-0x0C7
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0C8-0x0CB
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0CC-0x0CF
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0D0-0x0D3
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0D4-0x0D7
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0D8-0x0DB
    &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  &Interpreter::addImm8T,  // 0x0DC-0x0DF
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0E0-0x0E3
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0E4-0x0E7
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0E8-0x0EB
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0EC-0x0EF
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0F0-0x0F3
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0F4-0x0F7
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0F8-0x0FB
    &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  &Interpreter::subImm8T,  // 0x0FC-0x0FF
    &Interpreter::andDpT,    &Interpreter::eorDpT,    &Interpreter::lslDpT,    &Interpreter::lsrDpT,    // 0x100-0x103
    &Interpreter::asrDpT,    &Interpreter::adcDpT,    &Interpreter::sbcDpT,    &Interpreter::rorDpT,    // 0x104-0x107
    &Interpreter::tstDpT,    &Interpreter::negDpT,    &Interpreter::cmpDpT,    &Interpreter::cmnDpT,    // 0x108-0x10B
    &Interpreter::orrDpT,    &Interpreter::mulDpT,    &Interpreter::bicDpT,    &Interpreter::mvnDpT,    // 0x10C-0x10F
    &Interpreter::addHT,     &Interpreter::addHT,     &Interpreter::addHT,     &Interpreter::addHT,     // 0x110-0x113
    &Interpreter::cmpHT,     &Interpreter::cmpHT,     &Interpreter::cmpHT,     &Interpreter::cmpHT,     // 0x114-0x117
    &Interpreter::movHT,     &Interpreter::movHT,     &Interpreter::movHT,     &Interpreter::movHT,     // 0x118-0x11B
    &Interpreter::bxRegT,    &Interpreter::bxRegT,    &Interpreter::blxRegT,   &Interpreter::blxRegT,   // 0x11C-0x11F
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x120-0x123
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x124-0x127
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x128-0x12B
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x12C-0x12F
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x130-0x133
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x134-0x137
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x138-0x13B
    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    &Interpreter::ldrPcT,    // 0x13C-0x13F
    &Interpreter::strRegT,   &Interpreter::strRegT,   &Interpreter::strRegT,   &Interpreter::strRegT,   // 0x140-0x143
    &Interpreter::strRegT,   &Interpreter::strRegT,   &Interpreter::strRegT,   &Interpreter::strRegT,   // 0x144-0x147
    &Interpreter::strhRegT,  &Interpreter::strhRegT,  &Interpreter::strhRegT,  &Interpreter::strhRegT,  // 0x148-0x14B
    &Interpreter::strhRegT,  &Interpreter::strhRegT,  &Interpreter::strhRegT,  &Interpreter::strhRegT,  // 0x14C-0x14F
    &Interpreter::strbRegT,  &Interpreter::strbRegT,  &Interpreter::strbRegT,  &Interpreter::strbRegT,  // 0x150-0x153
    &Interpreter::strbRegT,  &Interpreter::strbRegT,  &Interpreter::strbRegT,  &Interpreter::strbRegT,  // 0x154-0x157
    &Interpreter::ldrsbRegT, &Interpreter::ldrsbRegT, &Interpreter::ldrsbRegT, &Interpreter::ldrsbRegT, // 0x158-0x15B
    &Interpreter::ldrsbRegT, &Interpreter::ldrsbRegT, &Interpreter::ldrsbRegT, &Interpreter::ldrsbRegT, // 0x15C-0x15F
    &Interpreter::ldrRegT,   &Interpreter::ldrRegT,   &Interpreter::ldrRegT,   &Interpreter::ldrRegT,   // 0x160-0x163
    &Interpreter::ldrRegT,   &Interpreter::ldrRegT,   &Interpreter::ldrRegT,   &Interpreter::ldrRegT,   // 0x164-0x167
    &Interpreter::ldrhRegT,  &Interpreter::ldrhRegT,  &Interpreter::ldrhRegT,  &Interpreter::ldrhRegT,  // 0x168-0x16B
    &Interpreter::ldrhRegT,  &Interpreter::ldrhRegT,  &Interpreter::ldrhRegT,  &Interpreter::ldrhRegT,  // 0x16C-0x16F
    &Interpreter::ldrbRegT,  &Interpreter::ldrbRegT,  &Interpreter::ldrbRegT,  &Interpreter::ldrbRegT,  // 0x170-0x173
    &Interpreter::ldrbRegT,  &Interpreter::ldrbRegT,  &Interpreter::ldrbRegT,  &Interpreter::ldrbRegT,  // 0x174-0x177
    &Interpreter::ldrshRegT, &Interpreter::ldrshRegT, &Interpreter::ldrshRegT, &Interpreter::ldrshRegT, // 0x178-0x17B
    &Interpreter::ldrshRegT, &Interpreter::ldrshRegT, &Interpreter::ldrshRegT, &Interpreter::ldrshRegT, // 0x17C-0x17F
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x180-0x183
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x184-0x187
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x188-0x18B
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x18C-0x18F
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x190-0x193
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x194-0x197
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x198-0x19B
    &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  &Interpreter::strImm5T,  // 0x19C-0x19F
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1A0-0x1A3
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1A4-0x1A7
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1A8-0x1AB
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1AC-0x1AF
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1B0-0x1B3
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1B4-0x1B7
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1B8-0x1BB
    &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  &Interpreter::ldrImm5T,  // 0x1BC-0x1BF
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1C0-0x1C3
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1C4-0x1C7
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1C8-0x1CB
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1CC-0x1CF
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1D0-0x1D3
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1D4-0x1D7
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1D8-0x1DB
    &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, &Interpreter::strbImm5T, // 0x1DC-0x1DF
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1E0-0x1E3
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1E4-0x1E7
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1E8-0x1EB
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1EC-0x1EF
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1F0-0x1F3
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1F4-0x1F7
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1F8-0x1FB
    &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, &Interpreter::ldrbImm5T, // 0x1FC-0x1FF
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x200-0x203
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x204-0x207
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x208-0x20B
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x20C-0x20F
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x210-0x213
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x214-0x217
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x218-0x21B
    &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, &Interpreter::strhImm5T, // 0x21C-0x21F
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x220-0x223
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x224-0x227
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x228-0x22B
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x22C-0x22F
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x230-0x233
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x234-0x237
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x238-0x23B
    &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, &Interpreter::ldrhImm5T, // 0x23C-0x23F
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x240-0x243
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x244-0x247
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x248-0x24B
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x24C-0x24F
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x250-0x253
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x254-0x257
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x258-0x25B
    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    &Interpreter::strSpT,    // 0x25C-0x25F
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x260-0x263
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x264-0x267
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x268-0x26B
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x26C-0x26F
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x270-0x273
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x274-0x277
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x278-0x27B
    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    &Interpreter::ldrSpT,    // 0x27C-0x27F
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x280-0x283
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x284-0x287
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x288-0x28B
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x28C-0x28F
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x290-0x293
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x294-0x297
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x298-0x29B
    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    &Interpreter::addPcT,    // 0x29C-0x29F
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2A0-0x2A3
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2A4-0x2A7
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2A8-0x2AB
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2AC-0x2AF
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2B0-0x2B3
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2B4-0x2B7
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2B8-0x2BB
    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    &Interpreter::addSpT,    // 0x2BC-0x2BF
    &Interpreter::addSpImmT, &Interpreter::addSpImmT, &Interpreter::addSpImmT, &Interpreter::addSpImmT, // 0x2C0-0x2C3
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2C4-0x2C7
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2C8-0x2CB
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2CC-0x2CF
    &Interpreter::pushT,     &Interpreter::pushT,     &Interpreter::pushT,     &Interpreter::pushT,     // 0x2D0-0x2D3
    &Interpreter::pushLrT,   &Interpreter::pushLrT,   &Interpreter::pushLrT,   &Interpreter::pushLrT,   // 0x2D4-0x2D7
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2D8-0x2DB
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2DC-0x2DF
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2E0-0x2E3
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2E4-0x2E7
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2E8-0x2EB
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2EC-0x2EF
    &Interpreter::popT,      &Interpreter::popT,      &Interpreter::popT,      &Interpreter::popT,      // 0x2F0-0x2F3
    &Interpreter::popPcT,    &Interpreter::popPcT,    &Interpreter::popPcT,    &Interpreter::popPcT,    // 0x2F4-0x2F7
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2F8-0x2FB
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x2FC-0x2FF
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x300-0x303
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x304-0x307
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x308-0x30B
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x30C-0x30F
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x310-0x313
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x314-0x317
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x318-0x31B
    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    &Interpreter::stmiaT,    // 0x31C-0x31F
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x320-0x323
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x324-0x327
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x328-0x32B
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x32C-0x32F
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x330-0x333
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x334-0x337
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x338-0x33B
    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    &Interpreter::ldmiaT,    // 0x33C-0x33F
    &Interpreter::beqT,      &Interpreter::beqT,      &Interpreter::beqT,      &Interpreter::beqT,      // 0x340-0x343
    &Interpreter::bneT,      &Interpreter::bneT,      &Interpreter::bneT,      &Interpreter::bneT,      // 0x344-0x347
    &Interpreter::bcsT,      &Interpreter::bcsT,      &Interpreter::bcsT,      &Interpreter::bcsT,      // 0x348-0x34B
    &Interpreter::bccT,      &Interpreter::bccT,      &Interpreter::bccT,      &Interpreter::bccT,      // 0x34C-0x34F
    &Interpreter::bmiT,      &Interpreter::bmiT,      &Interpreter::bmiT,      &Interpreter::bmiT,      // 0x350-0x353
    &Interpreter::bplT,      &Interpreter::bplT,      &Interpreter::bplT,      &Interpreter::bplT,      // 0x354-0x357
    &Interpreter::bvsT,      &Interpreter::bvsT,      &Interpreter::bvsT,      &Interpreter::bvsT,      // 0x358-0x35B
    &Interpreter::bvcT,      &Interpreter::bvcT,      &Interpreter::bvcT,      &Interpreter::bvcT,      // 0x35C-0x35F
    &Interpreter::bhiT,      &Interpreter::bhiT,      &Interpreter::bhiT,      &Interpreter::bhiT,      // 0x360-0x363
    &Interpreter::blsT,      &Interpreter::blsT,      &Interpreter::blsT,      &Interpreter::blsT,      // 0x364-0x367
    &Interpreter::bgeT,      &Interpreter::bgeT,      &Interpreter::bgeT,      &Interpreter::bgeT,      // 0x368-0x36B
    &Interpreter::bltT,      &Interpreter::bltT,      &Interpreter::bltT,      &Interpreter::bltT,      // 0x36C-0x36F
    &Interpreter::bgtT,      &Interpreter::bgtT,      &Interpreter::bgtT,      &Interpreter::bgtT,      // 0x370-0x373
    &Interpreter::bleT,      &Interpreter::bleT,      &Interpreter::bleT,      &Interpreter::bleT,      // 0x374-0x377
    &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  &Interpreter::unkThumb,  // 0x378-0x37B
    &Interpreter::swiT,      &Interpreter::swiT,      &Interpreter::swiT,      &Interpreter::swiT,      // 0x37C-0x37F
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x380-0x383
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x384-0x387
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x388-0x38B
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x38C-0x38F
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x390-0x393
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x394-0x397
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x398-0x39B
    &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        &Interpreter::bT,        // 0x39C-0x39F
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3A0-0x3A3
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3A4-0x3A7
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3A8-0x3AB
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3AC-0x3AF
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3B0-0x3B3
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3B4-0x3B7
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3B8-0x3BB
    &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   &Interpreter::blxOffT,   // 0x3BC-0x3BF
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3C0-0x3C3
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3C4-0x3C7
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3C8-0x3CB
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3CC-0x3CF
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3D0-0x3D3
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3D4-0x3D7
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3D8-0x3DB
    &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  &Interpreter::blSetupT,  // 0x3DC-0x3DF
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    // 0x3E0-0x3E3
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    // 0x3E4-0x3E7
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    // 0x3E8-0x3EB
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    // 0x3EC-0x3EF
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    // 0x3F0-0x3F3
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    // 0x3F4-0x3F7
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    // 0x3F8-0x3FB
    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT,    &Interpreter::blOffT     // 0x3FC-0x3FF
};

// Precomputed ARM condition evaluations; index bits 7-4 are condition code, bits 3-0 are NZCV
const uint8_t Interpreter::condition[] =
{
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, // EQ
    1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, // NE
    0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, // CS
    1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, // CC
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, // MI
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, // PL
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, // VS
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, // VC
    0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, // HI
    1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, // LS
    1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, // GE
    0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, // LT
    1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, // GT
    0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, // LE
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // AL
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2  // Reserved
};

Interpreter::Interpreter(Core *core, bool cpu): core(core), cpu(cpu)
{
    for (int i = 0; i < 16; i++)
        registers[i] = &registersUsr[i];

    // Prepare tasks to be used with the scheduler
    interruptTask = std::bind(&Interpreter::interrupt, this);
}

void Interpreter::init()
{
    // Prepare to boot the BIOS
    setCpsr(0x000000D3); // Supervisor, interrupts off
    registersUsr[15] = (cpu == 0) ? 0xFFFF0000 : 0x00000000;
    flushPipeline();

    // Reset the registers
    ime = 0;
    ie = irf = 0;
    postFlg = 0;
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

    setCpsr(0x000000DF); // System, interrupts off
    registersUsr[12] = entryAddr;
    registersUsr[14] = entryAddr;
    registersUsr[15] = entryAddr;
    flushPipeline();
}

void Interpreter::sendInterrupt(int bit)
{
    // Set the interrupt's request bit
    irf |= BIT(bit);

    // Trigger an interrupt if the conditions are met, or unhalt the CPU even if interrupts are disabled
    // The ARM9 additionally needs IME to be set for it to unhalt, but the ARM7 doesn't care
    if (ie & irf)
    {
        if (ime && !(cpsr & BIT(7)))
            core->schedule(Task(&interruptTask, (cpu == 1 && !core->isGbaMode()) ? 2 : 1));
        else if (ime || cpu == 1)
            halted &= ~BIT(0);
    }
}

void Interpreter::interrupt()
{
    // Perform an interrupt if the conditions still hold
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
    {
        // Switch to interrupt mode, save the return address, and jump to the interrupt handler
        setCpsr((cpsr & ~0x3F) | 0x92, true); // ARM, IRQ, interrupts off
        *registers[14] = *registers[15] + ((*spsr & BIT(5)) ? 2 : 0);
        *registers[15] = ((cpu == 0) ? core->cp15.getExceptionAddr() : 0x00000000) + 0x18;
        flushPipeline();

        // Unhalt the CPU
        halted &= ~BIT(0);
    }
}

void Interpreter::flushPipeline()
{
    // Adjust the program counter and refill the pipeline after a jump
    if (cpsr & BIT(5)) // THUMB mode
    {
        *registers[15] = (*registers[15] & ~1) + 2;
        pipeline[0] = core->memory.read<uint16_t>(cpu, *registers[15] - 2);
        pipeline[1] = core->memory.read<uint16_t>(cpu, *registers[15]);
    }
    else // ARM mode
    {
        *registers[15] = (*registers[15] & ~3) + 4;
        pipeline[0] = core->memory.read<uint32_t>(cpu, *registers[15] - 4);
        pipeline[1] = core->memory.read<uint32_t>(cpu, *registers[15]);
    }
}

void Interpreter::setCpsr(uint32_t value, bool save)
{
    // Swap banked registers if the CPU mode changed
    if ((value & 0x1F) != (cpsr & 0x1F))
    {
        switch (value & 0x1F)
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
                LOG("Unknown ARM%d CPU mode: 0x%X\n", ((cpu == 0) ? 9 : 7), value & 0x1F);
                break;
        }
    }

    // Set the CPSR, saving the old value if requested
    if (save && spsr) *spsr = cpsr;
    cpsr = value;

    // Trigger an interrupt if the conditions are met
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
        core->schedule(Task(&interruptTask, (cpu == 1 && !core->isGbaMode()) ? 2 : 1));
}

int Interpreter::handleReserved(uint32_t opcode)
{
    // The ARM9-exclusive BLX instruction uses the reserved condition code, so let it run
    if ((opcode & 0x0E000000) == 0x0A000000)
        return blx(opcode); // BLX label

    // If a DLDI function was jumped to, HLE it and return
    if (core->dldi.isPatched())
    {
        switch (opcode)
        {
            case DLDI_START:  *registers[0] = core->dldi.startup();                                                      break;
            case DLDI_INSERT: *registers[0] = core->dldi.isInserted();                                                   break;
            case DLDI_READ:   *registers[0] = core->dldi.readSectors(cpu, *registers[0], *registers[1], *registers[2]);  break;
            case DLDI_WRITE:  *registers[0] = core->dldi.writeSectors(cpu, *registers[0], *registers[1], *registers[2]); break;
            case DLDI_CLEAR:  *registers[0] = core->dldi.clearStatus();                                                  break;
            case DLDI_STOP:   *registers[0] = core->dldi.shutdown();                                                     break;
        }
        return bx(14);
    }

    return unkArm(opcode);
}

int Interpreter::unkArm(uint32_t opcode)
{
    // Handle an unknown ARM opcode
    LOG("Unknown ARM%d ARM opcode: 0x%X\n", ((cpu == 0) ? 9 : 7), opcode);
    return 1;
}

int Interpreter::unkThumb(uint16_t opcode)
{
    // Handle an unknown THUMB opcode
    LOG("Unknown ARM%d THUMB opcode: 0x%X\n", ((cpu == 0) ? 9 : 7), opcode);
    return 1;
}

void Interpreter::writeIme(uint8_t value)
{
    // Write to the IME register
    ime = value & 0x01;

    // Trigger an interrupt if the conditions are met
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
        core->schedule(Task(&interruptTask, (cpu == 1 && !core->isGbaMode()) ? 2 : 1));
}

void Interpreter::writeIe(uint32_t mask, uint32_t value)
{
    // Write to the IE register
    mask &= ((cpu == 0) ? 0x003F3F7F : (core->isGbaMode() ? 0x3FFF : 0x01FF3FFF));
    ie = (ie & ~mask) | (value & mask);

    // Trigger an interrupt if the conditions are met
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
        core->schedule(Task(&interruptTask, (cpu == 1 && !core->isGbaMode()) ? 2 : 1));
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
