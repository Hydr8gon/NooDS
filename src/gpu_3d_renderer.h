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

#include <cstdint>
#include <thread>

class Gpu3D;
struct Vertex;
struct _Polygon;

class Gpu3DRenderer
{
    public:
        Gpu3DRenderer(Gpu3D *gpu3D): gpu3D(gpu3D) {};
        ~Gpu3DRenderer();

        void drawScanline(int line);

        uint32_t *getFramebuffer() { return framebuffer; }

        void setTexture(int slot, uint8_t *data) { textures[slot] = data; }
        void setPalette(int slot, uint8_t *data) { palettes[slot] = data; }

        uint16_t readDisp3DCnt() { return disp3DCnt; }

        void writeDisp3DCnt(uint16_t mask, uint16_t value);
        void writeClearColor(uint32_t mask, uint32_t value);
        void writeClearDepth(uint16_t mask, uint16_t value);
        void writeFogColor(uint32_t mask, uint32_t value);
        void writeFogOffset(uint16_t mask, uint16_t value);
        void writeFogTable(int index, uint8_t value);
        void writeToonTable(int index, uint16_t mask, uint16_t value);

    private:
        uint32_t framebuffer[256 * 192] = {};
        int32_t depthBuffer[4][256] = {};
        uint8_t attribBuffer[4][256] = {};
        uint8_t stencilBuffer[4][256] = {};
        bool stencilClear[4] = {};

        std::thread *threads[4] = {};
        int activeThreads = 0;

        uint8_t *textures[4] = {};
        uint8_t *palettes[6] = {};

        uint16_t disp3DCnt = 0;
        uint32_t clearColor = 0;
        uint16_t clearDepth = 0;
        uint32_t fogColor = 0;
        uint16_t fogOffset = 0;
        uint8_t fogTable[32] = {};
        uint16_t toonTable[32] = {};

        Gpu3D *gpu3D;

        uint32_t rgba5ToRgba6(uint32_t color);

        void drawScanline48(int block);
        void drawScanline1(int line);

        uint8_t *getTexture(uint32_t address);
        uint8_t *getPalette(uint32_t address);

        int interpolate(int64_t v1, int64_t v2, int x1, int x, int x2);
        int interpolateW(int64_t w1, int64_t w2, int x1, int x, int x2);
        int interpolate(int64_t v1, int64_t v2, int x1, int x, int x2, int w1, int w2);
        uint32_t interpolateColor(uint32_t c1, uint32_t c2, int x1, int x, int x2);

        uint32_t readTexture(_Polygon *polygon, int s, int t);

        void drawPolygon(int line, _Polygon *polygon);
        void rasterize(int line, _Polygon *polygon, Vertex *v1, Vertex *v2, Vertex *v3, Vertex *v4);
};

#endif // GPU_3D_RENDERER_H
