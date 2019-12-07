/*
    Copyright 2019 Hydr8gon

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

class Gpu3D;
struct Vertex;
struct _Polygon;

class Gpu3DRenderer
{
    public:
        Gpu3DRenderer(Gpu3D *gpu3D): gpu3D(gpu3D) {};

        void drawScanline(int line);

        uint16_t *getLineCache() { return lineCache; }

    private:
        uint16_t lineCache[48 * 256] = {};
        int zBuffer[256] = {};

        Gpu3D *gpu3D;

        Vertex normalize(Vertex vertex);
        int interpolate(int min, int max, int start, int current, int end);
        uint16_t interpolateColor(uint16_t min, uint16_t max, int start, int current, int end);
        uint16_t readTexture(_Polygon *polygon, int s, int t);
        void rasterize(int line, _Polygon *polygon, Vertex *v1, Vertex *v2, Vertex *v3, Vertex *v4);
};

#endif // GPU_3D_RENDERER_H
