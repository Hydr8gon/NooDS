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

#ifndef GPU_3D_RENDERER_H
#define GPU_3D_RENDERER_H

#include <atomic>
#include <cstdint>
#include <thread>

class Core;
struct Vertex;
struct _Polygon;

class Gpu3DRenderer
{
    public:
        Gpu3DRenderer(Core *core);
        ~Gpu3DRenderer();

        void drawScanline(int line);

        uint32_t *getFramebuffer(int line);

        uint16_t readDisp3DCnt() { return disp3DCnt; }

        void writeDisp3DCnt(uint16_t mask, uint16_t value);
        void writeClearColor(uint32_t mask, uint32_t value);
        void writeClearDepth(uint16_t mask, uint16_t value);
        void writeFogColor(uint32_t mask, uint32_t value);
        void writeFogOffset(uint16_t mask, uint16_t value);
        void writeFogTable(int index, uint8_t value);
        void writeToonTable(int index, uint16_t mask, uint16_t value);

    private:
        Core *core;

        uint32_t framebuffer[256 * 192] = {};
        uint32_t depthBuffer[3][256] = {};
        uint8_t attribBuffer[3][256] = {};
        uint8_t stencilBuffer[3][256] = {};
        bool stencilClear[3] = {};

        int polygonTop[2048] = {};
        int polygonBot[2048] = {};

        int activeThreads = 0;
        std::thread *threads[3] = {};
        std::atomic<bool> ready[192];

        uint16_t disp3DCnt = 0;
        uint32_t clearColor = 0;
        uint16_t clearDepth = 0;
        uint32_t fogColor = 0;
        uint16_t fogOffset = 0;
        uint8_t fogTable[32] = {};
        uint16_t toonTable[32] = {};

        static uint32_t rgba5ToRgba6(uint32_t color);

        void drawThreaded(int thread);
        void drawScanline1(int line, int thread);

        uint8_t *getTexture(uint32_t address);
        uint8_t *getPalette(uint32_t address);

        static uint32_t interpolateLinear(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2);
        static uint32_t interpolateLinRev(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2);
        static uint32_t interpolateFill(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2, uint32_t w1, uint32_t w2);
        static uint32_t interpolateEdge(uint32_t v1, uint32_t v2, uint32_t x1, uint32_t x, uint32_t x2, uint32_t w1, uint32_t w2);
        static uint32_t interpolateColor(uint32_t c1, uint32_t c2, uint32_t x1, uint32_t x, uint32_t x2);

        uint32_t readTexture(_Polygon *polygon, int s, int t);
        void drawPolygon(int line, int thread, int polygonIndex);
};

#endif // GPU_3D_RENDERER_H
