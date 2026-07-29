/*
    Copyright 2019-2026 Hydr8gon

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

#pragma once

#include <cstdint>
#include <deque>

class Core;

class Aes {
public:
    Aes(Core *core);
    void saveState(FILE *file);
    void loadState(FILE *file);
    void update();

    uint32_t readCnt() { return aesCnt; }
    uint32_t readRdfifo();

    void writeCnt(uint32_t mask, uint32_t value);
    void writeBlkcnt(uint32_t mask, uint32_t value);
    void writeWrfifo(uint32_t mask, uint32_t value);
    void writeIv(int i, uint32_t mask, uint32_t value);
    void writeMac(int i, uint32_t mask, uint32_t value);
    void writeKey(int i, int j, uint32_t mask, uint32_t value);
    void writeKeyx(int i, int j, uint32_t mask, uint32_t value);
    void writeKeyy(int i, int j, uint32_t mask, uint32_t value);

private:
    Core *core;
    bool scheduled = false;

    uint8_t fsBox[0x100];
    uint8_t rsBox[0x100];
    uint32_t fTable[0x100];
    uint32_t rTable[0x100];

    uint32_t keys[4][4] = {};
    uint32_t keysX[4][4] = {};
    uint32_t keysY[4][4] = {};
    uint32_t rKey[44] = {};
    uint32_t ctr[4] = {};
    uint32_t cbc[4] = {};
    uint16_t curBlock = 0;
    uint16_t curExtra = 0;
    uint8_t curKey = 0;

    std::deque<uint32_t> writeFifo;
    std::deque<uint32_t> readFifo;

    uint32_t aesCnt = 0;
    uint32_t aesBlkcnt = 0;
    uint32_t aesRdfifo = 0;
    uint32_t aesIv[4] = {};
    uint32_t aesMac[4] = {};

    static uint32_t scatter8(uint8_t *t, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
    static uint32_t scatter32(uint32_t *t, uint32_t a, uint32_t b, uint32_t c, uint32_t d);

    static void rolKey(uint32_t *key, uint8_t rol);
    static void xorKey(uint32_t *dst, uint32_t *src);
    static void addKey(uint32_t *dst, uint32_t *src);

    void cryptBlock(uint32_t *src, uint32_t *dst);
    void initFifo();
    void triggerFifo();
};
