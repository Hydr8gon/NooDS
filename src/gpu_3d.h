/*
    Copyright 2019-2025 Hydr8gon

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
#include <queue>
#include <vector>

#include "defines.h"

class Core;

enum GXState {
    GX_IDLE = 0,
    GX_RUNNING,
    GX_HALTED
};

struct Entry {
    uint8_t command;
    uint32_t param;

    Entry(uint8_t command, uint32_t param): command(command), param(param) {}
};

struct Matrix {
    int32_t data[4 * 4] = {
        1 << 12, 0 << 12, 0 << 12, 0 << 12,
        0 << 12, 1 << 12, 0 << 12, 0 << 12,
        0 << 12, 0 << 12, 1 << 12, 0 << 12,
        0 << 12, 0 << 12, 0 << 12, 1 << 12
    };

    Matrix operator*(Matrix &mtx);
};

struct Vector {
    int32_t x = 0, y = 0, z = 0;

    int32_t operator*(Vector &vtr);
    Vector operator*(Matrix &mtx);
};

struct Vertex: Vector {
    int32_t w = 0;
    int16_t s = 0, t = 0;
    uint32_t color = 0;

    Vertex operator*(Matrix &mtx);
};

struct _Polygon {
    uint32_t textureAddr = 0, paletteAddr = 0;
    uint16_t sizeS = 0, sizeT = 0;
    uint8_t textureFmt = 0;
    bool repeatS = false, repeatT = false;
    bool flipS = false, flipT = false;
    bool transparent0 = false;

    uint16_t vertices = 0;
    uint8_t size = 0;
    int8_t wShift = 0;
    bool wBuffer = false;
    bool crossed = false;
    bool clockwise = false;

    uint8_t mode = 0;
    uint8_t alpha = 0;
    uint8_t id = 0;
    bool transNewDepth = false;
    bool depthTestEqual = false;
    bool fog = false;
};

class Gpu3D {
public:
    _Polygon *polygonsOut = polygons2;
    Vertex *verticesOut = vertices2;
    uint16_t polygonCountOut = 0;
    uint16_t vertexCountOut = 0;

    Gpu3D(Core *core): core(core) {}
    void saveState(FILE *file);
    void loadState(FILE *file);

    void runCommands();
    void swapBuffers();
    bool shouldSwap() { return state == GX_HALTED; }

    uint32_t readGxStat() { return gxStat; }
    uint32_t readPosResult(int index) { return posResult[index]; }
    uint32_t readVecResult(int index) { return vecResult[index]; }
    uint32_t readRamCount();
    uint32_t readClipMtxResult(int index);
    uint32_t readVecMtxResult(int index);

    void writeGxFifo(uint32_t mask, uint32_t value);
    void writeMtxMode(uint32_t mask, uint32_t value);
    void writeMtxPush(uint32_t mask, uint32_t value);
    void writeMtxPop(uint32_t mask, uint32_t value);
    void writeMtxStore(uint32_t mask, uint32_t value);
    void writeMtxRestore(uint32_t mask, uint32_t value);
    void writeMtxIdentity(uint32_t mask, uint32_t value);
    void writeMtxLoad44(uint32_t mask, uint32_t value);
    void writeMtxLoad43(uint32_t mask, uint32_t value);
    void writeMtxMult44(uint32_t mask, uint32_t value);
    void writeMtxMult43(uint32_t mask, uint32_t value);
    void writeMtxMult33(uint32_t mask, uint32_t value);
    void writeMtxScale(uint32_t mask, uint32_t value);
    void writeMtxTrans(uint32_t mask, uint32_t value);
    void writeColor(uint32_t mask, uint32_t value);
    void writeNormal(uint32_t mask, uint32_t value);
    void writeTexCoord(uint32_t mask, uint32_t value);
    void writeVtx16(uint32_t mask, uint32_t value);
    void writeVtx10(uint32_t mask, uint32_t value);
    void writeVtxXY(uint32_t mask, uint32_t value);
    void writeVtxXZ(uint32_t mask, uint32_t value);
    void writeVtxYZ(uint32_t mask, uint32_t value);
    void writeVtxDiff(uint32_t mask, uint32_t value);
    void writePolygonAttr(uint32_t mask, uint32_t value);
    void writeTexImageParam(uint32_t mask, uint32_t value);
    void writePlttBase(uint32_t mask, uint32_t value);
    void writeDifAmb(uint32_t mask, uint32_t value);
    void writeSpeEmi(uint32_t mask, uint32_t value);
    void writeLightVector(uint32_t mask, uint32_t value);
    void writeLightColor(uint32_t mask, uint32_t value);
    void writeShininess(uint32_t mask, uint32_t value);
    void writeBeginVtxs(uint32_t mask, uint32_t value);
    void writeEndVtxs(uint32_t mask, uint32_t value);
    void writeSwapBuffers(uint32_t mask, uint32_t value);
    void writeViewport(uint32_t mask, uint32_t value);
    void writeBoxTest(uint32_t mask, uint32_t value);
    void writePosTest(uint32_t mask, uint32_t value);
    void writeVecTest(uint32_t mask, uint32_t value);
    void writeGxStat(uint32_t mask, uint32_t value);

private:
    Core *core;

    GXState state = GX_IDLE;

    std::deque<Entry> fifo;
    uint32_t pipeSize = 0;
    uint32_t testQueue = 0;
    uint32_t matrixQueue = 0;

    static const uint8_t paramCounts[0x100];

    uint8_t matrixMode = 0;
    bool clipDirty = false;

    Matrix projection, projectionStack;
    Matrix coordinate, coordinateStack[32];
    Matrix direction, directionStack[32];
    Matrix texture, textureStack;
    Matrix clip;

    Vertex vertices1[6144], vertices2[6144];
    Vertex *verticesIn = vertices1;
    uint16_t vertexCountIn = 0;
    uint16_t processCount = 0;

    _Polygon polygons1[2048], polygons2[2048];
    _Polygon *polygonsIn = polygons1;
    uint16_t polygonCountIn = 0;

    Vertex savedVertex;
    _Polygon savedPolygon;
    int16_t s, t;

    int vertexCount = 0;
    bool clockwise = false;
    int polygonType = 0;
    int textureCoordMode = 0;

    uint32_t polygonAttr = 0;
    uint8_t enabledLights = 0;
    bool renderBack = false, renderFront = false;

    uint32_t diffuseColor = 0, ambientColor = 0;
    uint32_t specularColor = 0, emissionColor = 0;
    bool shininessEnabled = false;
    Vector lightVector[4], halfVector[4];
    uint32_t lightColor[4] = {};
    uint8_t shininess[128] = {};

    uint16_t viewport[4] = { 0, 0, 256, 192 };
    uint16_t viewportNext[4] = { 0, 0, 256, 192 };

    uint32_t gxFifo = 0x00000000;
    uint32_t gxStat = 0x04000000;
    int32_t posResult[4] = {};
    int16_t vecResult[3] = {};

    uint8_t gxFifoCount = 0;

    static uint32_t rgb5ToRgb6(uint16_t color);
    static Vertex intersection(Vertex *vtx1, Vertex *vtx2, int32_t val1, int32_t val2);
    static bool clipPolygon(Vertex *unclipped, Vertex *clipped, uint8_t *size);

    void processVertices();
    void addVertex();
    void addPolygon();

    void mtxModeCmd(uint32_t param);
    void mtxPushCmd();
    void mtxPopCmd(uint32_t param);
    void mtxStoreCmd(uint32_t param);
    void mtxRestoreCmd(uint32_t param);
    void mtxIdentityCmd();
    void mtxLoad44Cmd(std::vector<uint32_t> &params);
    void mtxLoad43Cmd(std::vector<uint32_t> &params);
    void mtxMult44Cmd(std::vector<uint32_t> &params);
    void mtxMult43Cmd(std::vector<uint32_t> &params);
    void mtxMult33Cmd(std::vector<uint32_t> &params);
    void mtxScaleCmd(std::vector<uint32_t> &params);
    void mtxTransCmd(std::vector<uint32_t> &params);
    void colorCmd(uint32_t param);
    void normalCmd(uint32_t param);
    void texCoordCmd(uint32_t param);
    void vtx16Cmd(std::vector<uint32_t> &params);
    void vtx10Cmd(uint32_t param);
    void vtxXYCmd(uint32_t param);
    void vtxXZCmd(uint32_t param);
    void vtxYZCmd(uint32_t param);
    void vtxDiffCmd(uint32_t param);
    void polygonAttrCmd(uint32_t param);
    void texImageParamCmd(uint32_t param);
    void plttBaseCmd(uint32_t param);
    void difAmbCmd(uint32_t param);
    void speEmiCmd(uint32_t param);
    void lightVectorCmd(uint32_t param);
    void lightColorCmd(uint32_t param);
    void shininessCmd(std::vector<uint32_t> &params);
    void beginVtxsCmd(uint32_t param);
    void swapBuffersCmd(uint32_t param);
    void viewportCmd(uint32_t param);
    void boxTestCmd(std::vector<uint32_t> &params);
    void posTestCmd(std::vector<uint32_t> &params);
    void vecTestCmd(uint32_t param);

    void addEntry(Entry entry);
};
