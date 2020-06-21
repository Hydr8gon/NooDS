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

#ifndef GPU_3D_H
#define GPU_3D_H

#include <cstdint>
#include <queue>

#include "defines.h"

class Dma;
class Interpreter;

struct Entry
{
    Entry(uint8_t command, uint32_t param): command(command), param(param) {}

    uint8_t command;
    uint32_t param;
};

struct Matrix
{
    int64_t data[4 * 4] =
    {
        1 << 12, 0 << 12, 0 << 12, 0 << 12,
        0 << 12, 1 << 12, 0 << 12, 0 << 12,
        0 << 12, 0 << 12, 1 << 12, 0 << 12,
        0 << 12, 0 << 12, 0 << 12, 1 << 12
    };
};

struct Vertex
{
    int64_t x = 0, y = 0, z = 0, w = 0;
    int64_t s = 0, t = 0;
    uint32_t color = 0;
};

struct _Polygon
{
    int size = 0;
    Vertex *vertices = nullptr;
    bool crossed = false;

    int mode = 0;
    bool transNewDepth = false;
    bool depthTestEqual = false;
    bool fog = false;
    uint8_t alpha = 0;
    int id = 0;

    uint32_t textureAddr = 0, paletteAddr = 0;
    int sizeS = 0, sizeT = 0;
    bool repeatS = false, repeatT = false;
    bool flipS = false, flipT = false;
    int textureFmt = 0;
    bool transparent0 = false;

    bool wBuffer = false;
};


class Gpu3D
{
    public:
        Gpu3D(Dma *dma9, Interpreter *arm9);

        void runCycle();
        void swapBuffers();

        bool shouldRun()  { return !halted && (gxStat & BIT(27)); }
        bool shouldSwap() { return  halted;                       }

        _Polygon *getPolygons()     { return polygonsOut;     }
        int       getPolygonCount() { return polygonCountOut; }

        uint32_t readGxStat()             { return gxStat;           }
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
        bool halted = false;

        std::queue<Entry> fifo, pipe;

        int paramCounts[0x100] = {};
        int paramCount = 0;

        int matrixMode = 0;
        int projectionPtr = 0, coordinatePtr = 0;
        bool clipDirty = false;

        Matrix projection, projectionStack;
        Matrix coordinate, coordinateStack[32];
        Matrix direction, directionStack[32];
        Matrix texture, textureStack;
        Matrix clip;
        Matrix temp;

        Vertex vertices1[6144], vertices2[6144];
        Vertex *verticesIn = vertices1, *verticesOut = vertices2;
        int vertexCountIn = 0, vertexCountOut = 0;

        _Polygon polygons1[2048], polygons2[2048];
        _Polygon *polygonsIn = polygons1, *polygonsOut = polygons2;
        int polygonCountIn = 0, polygonCountOut = 0;

        Vertex savedVertex;
        _Polygon savedPolygon;
        int64_t s, t;

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
        Vertex lightVector[4], halfVector[4];
        uint32_t lightColor[4] = {};
        uint8_t shininess[128] = {};

        int viewportX = 0, viewportY = 0;
        int viewportWidth = 256, viewportHeight = 192;

        int16_t boxTestCoords[6] = {};

        uint32_t gxFifo = 0x00000000;
        uint32_t gxStat = 0x04000000;
        int32_t posResult[4] = {};
        int16_t vecResult[3] = {};

        int gxFifoCount = 0;

        Dma *dma9;
        Interpreter *arm9;

        Matrix multiply(Matrix *mtx1, Matrix *mtx2);
        Vertex multiply(Vertex *vtx, Matrix *mtx);
        int64_t multiply(Vertex *vec1, Vertex *vec2);

        uint32_t rgb5ToRgb6(uint16_t color);

        void addVertex();
        void addPolygon();

        Vertex intersection(Vertex *vtx1, Vertex *vtx2, int64_t val1, int64_t val2);
        bool clipPolygon(Vertex *unclipped, Vertex *clipped, int *size);

        void mtxModeCmd(uint32_t param);
        void mtxPushCmd();
        void mtxPopCmd(uint32_t param);
        void mtxStoreCmd(uint32_t param);
        void mtxRestoreCmd(uint32_t param);
        void mtxIdentityCmd();
        void mtxLoad44Cmd(uint32_t param);
        void mtxLoad43Cmd(uint32_t param);
        void mtxMult44Cmd(uint32_t param);
        void mtxMult43Cmd(uint32_t param);
        void mtxMult33Cmd(uint32_t param);
        void mtxScaleCmd(uint32_t param);
        void mtxTransCmd(uint32_t param);
        void colorCmd(uint32_t param);
        void normalCmd(uint32_t param);
        void texCoordCmd(uint32_t param);
        void vtx16Cmd(uint32_t param);
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
        void shininessCmd(uint32_t param);
        void beginVtxsCmd(uint32_t param);
        void swapBuffersCmd(uint32_t param);
        void viewportCmd(uint32_t param);
        void boxTestCmd(uint32_t param);
        void posTestCmd(uint32_t param);
        void vecTestCmd(uint32_t param);

        void addEntry(Entry entry);
};

#endif // GPU_3D_H

