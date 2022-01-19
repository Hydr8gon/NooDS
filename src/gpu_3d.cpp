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

#include <cstring>

#include "gpu_3d.h"
#include "core.h"

Matrix Matrix::operator*(Matrix &mtx)
{
    Matrix result;

    // Multiply 2 matrices
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            result.data[y * 4 + x] = ((int64_t)data[y * 4 + 0] * mtx.data[0 + x] + (int64_t)data[y * 4 + 1] * mtx.data[4  + x] +
                                      (int64_t)data[y * 4 + 2] * mtx.data[8 + x] + (int64_t)data[y * 4 + 3] * mtx.data[12 + x]) >> 12;
        }
    }

    return result;
}

int32_t Vector::operator*(Vector &vtr)
{
    // Multiply 2 vectors
    return ((int64_t)x * vtr.x + (int64_t)y * vtr.y + (int64_t)z * vtr.z) >> 12;
}

Vector Vector::operator*(Matrix &mtx)
{
    Vector result;

    // Multiply a vector with a matrix
    result.x = ((int64_t)x * mtx.data[0] + (int64_t)y * mtx.data[4] + (int64_t)z * mtx.data[8])  >> 12;
    result.y = ((int64_t)x * mtx.data[1] + (int64_t)y * mtx.data[5] + (int64_t)z * mtx.data[9])  >> 12;
    result.z = ((int64_t)x * mtx.data[2] + (int64_t)y * mtx.data[6] + (int64_t)z * mtx.data[10]) >> 12;

    return result;
}

Vertex Vertex::operator*(Matrix &mtx)
{
    Vertex result = *this;

    // Multiply a vertex with a matrix
    result.x = ((int64_t)x * mtx.data[0] + (int64_t)y * mtx.data[4] + (int64_t)z * mtx.data[8]  + (int64_t)w * mtx.data[12]) >> 12;
    result.y = ((int64_t)x * mtx.data[1] + (int64_t)y * mtx.data[5] + (int64_t)z * mtx.data[9]  + (int64_t)w * mtx.data[13]) >> 12;
    result.z = ((int64_t)x * mtx.data[2] + (int64_t)y * mtx.data[6] + (int64_t)z * mtx.data[10] + (int64_t)w * mtx.data[14]) >> 12;
    result.w = ((int64_t)x * mtx.data[3] + (int64_t)y * mtx.data[7] + (int64_t)z * mtx.data[11] + (int64_t)w * mtx.data[15]) >> 12;

    return result;
}

const uint8_t Gpu3D::paramCounts[] =
{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x00-0x0F
    1,  0,  1,  1,  1,  0, 16, 12, 16, 12,  9,  3,  3,  0,  0,  0, // 0x10-0x1F
    1,  1,  1,  2,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0, // 0x20-0x2F
    1,  1,  1,  1, 32,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x30-0x3F
    1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x40-0x4F
    1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x50-0x5F
    1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x60-0x6F
    3,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0x70-0x7F
};

Gpu3D::Gpu3D(Core *core): core(core)
{
    // Prepare tasks to be used with the scheduler
    runCommandTask = std::bind(&Gpu3D::runCommand, this);
}

uint32_t Gpu3D::rgb5ToRgb6(uint16_t color)
{
    // Convert an RGB5 value to an RGB6 value (the way the 3D engine does it)
    uint8_t r = ((color >>  0) & 0x1F) * 2; if (r > 0) r++;
    uint8_t g = ((color >>  5) & 0x1F) * 2; if (g > 0) g++;
    uint8_t b = ((color >> 10) & 0x1F) * 2; if (b > 0) b++;
    return (b << 12) | (g << 6) | r;
}

Vertex Gpu3D::intersection(Vertex *vtx1, Vertex *vtx2, int32_t val1, int32_t val2)
{
    Vertex vertex;

    // Calculate the interpolation coefficients
    int64_t d1 = val1 + vtx1->w;
    int64_t d2 = val2 + vtx2->w;
    if (d2 == d1) return *vtx1;

    // Interpolate the vertex coordinates
    vertex.x = ((d2 * vtx1->x) - (d1 * vtx2->x)) / (d2 - d1);
    vertex.y = ((d2 * vtx1->y) - (d1 * vtx2->y)) / (d2 - d1);
    vertex.z = ((d2 * vtx1->z) - (d1 * vtx2->z)) / (d2 - d1);
    vertex.w = ((d2 * vtx1->w) - (d1 * vtx2->w)) / (d2 - d1);
    vertex.s = ((d2 * vtx1->s) - (d1 * vtx2->s)) / (d2 - d1);
    vertex.t = ((d2 * vtx1->t) - (d1 * vtx2->t)) / (d2 - d1);

    // Interpolate the vertex color
    uint8_t r = ((d2 * ((vtx1->color >>  0) & 0x3F)) - (d1 * ((vtx2->color >>  0) & 0x3F))) / (d2 - d1);
    uint8_t g = ((d2 * ((vtx1->color >>  6) & 0x3F)) - (d1 * ((vtx2->color >>  6) & 0x3F))) / (d2 - d1);
    uint8_t b = ((d2 * ((vtx1->color >> 12) & 0x3F)) - (d1 * ((vtx2->color >> 12) & 0x3F))) / (d2 - d1);
    vertex.color = (vtx1->color & 0xFC0000) | (b << 12) | (g << 6) | r;

    return vertex;
}

bool Gpu3D::clipPolygon(Vertex *unclipped, Vertex *clipped, int *size)
{
    bool clip = false;

    // Start with the original unclipped vertices
    Vertex vertices[10];
    memcpy(vertices, unclipped, *size * sizeof(Vertex));

    // Clip a polygon using the Sutherland-Hodgman algorithm
    for (int i = 0; i < 6; i++)
    {
        int oldSize = *size;
        *size = 0;

        for (int j = 0; j < oldSize; j++)
        {
            // Get the unclipped vertices 
            Vertex *current = &vertices[j];
            Vertex *previous = &vertices[(j - 1 + oldSize) % oldSize];

            // Choose which coordinates to check based on the current side being clipped against
            int32_t currentVal, previousVal;
            switch (i)
            {
                case 0: currentVal =  current->x; previousVal =  previous->x; break;
                case 1: currentVal = -current->x; previousVal = -previous->x; break;
                case 2: currentVal =  current->y; previousVal =  previous->y; break;
                case 3: currentVal = -current->y; previousVal = -previous->y; break;
                case 4: currentVal =  current->z; previousVal =  previous->z; break;
                case 5: currentVal = -current->z; previousVal = -previous->z; break;
            }

            // Add the clipped vertices
            if (currentVal >= -current->w) // Current vertex in bounds
            {
                if (previousVal < -previous->w) // Previous vertex not in bounds
                {
                    clipped[(*size)++] = intersection(current, previous, currentVal, previousVal);
                    clip = true;
                }

                clipped[(*size)++] = *current;
            }
            else if (previousVal >= -previous->w) // Previous vertex in bounds
            {
                clipped[(*size)++] = intersection(current, previous, currentVal, previousVal);
                clip = true;
            }
        }

        // Update the current vertices
        memcpy(vertices, clipped, *size * sizeof(Vertex));
    }

    return clip;
}

void Gpu3D::runCommand()
{
    // Fetch the next geometry command
    Entry entry = fifo.front();
    int count = paramCounts[entry.command];
    std::vector<uint32_t> params;

    // If the command has multiple parameters, fetch them all
    if (count > 1)
    {
        params.reserve(count);
        for (int i = 0; i < count; i++)
        {
            params.push_back(fifo.front().param);
            fifo.pop();
        }
    }
    else
    {
        count = 1;
        fifo.pop();
    }

    // Execute the geometry command
    switch (entry.command)
    {
        case 0x10: mtxModeCmd(entry.param);       break; // MTX_MODE
        case 0x11: mtxPushCmd();                  break; // MTX_PUSH
        case 0x12: mtxPopCmd(entry.param);        break; // MTX_POP
        case 0x13: mtxStoreCmd(entry.param);      break; // MTX_STORE
        case 0x14: mtxRestoreCmd(entry.param);    break; // MTX_RESTORE
        case 0x15: mtxIdentityCmd();              break; // MTX_IDENTITY
        case 0x16: mtxLoad44Cmd(params);          break; // MTX_LOAD_4x4
        case 0x17: mtxLoad43Cmd(params);          break; // MTX_LOAD_4x3
        case 0x18: mtxMult44Cmd(params);          break; // MTX_MULT_4x4
        case 0x19: mtxMult43Cmd(params);          break; // MTX_MULT_4x3
        case 0x1A: mtxMult33Cmd(params);          break; // MTX_MULT_3x3
        case 0x1B: mtxScaleCmd(params);           break; // MTX_SCALE
        case 0x1C: mtxTransCmd(params);           break; // MTX_TRANS
        case 0x20: colorCmd(entry.param);         break; // COLOR
        case 0x21: normalCmd(entry.param);        break; // NORMAL
        case 0x22: texCoordCmd(entry.param);      break; // TEXCOORD
        case 0x23: vtx16Cmd(params);              break; // VTX_16
        case 0x24: vtx10Cmd(entry.param);         break; // VTX_10
        case 0x25: vtxXYCmd(entry.param);         break; // VTX_XY
        case 0x26: vtxXZCmd(entry.param);         break; // VTX_XZ
        case 0x27: vtxYZCmd(entry.param);         break; // VTX_YZ
        case 0x28: vtxDiffCmd(entry.param);       break; // VTX_DIFF
        case 0x29: polygonAttrCmd(entry.param);   break; // POLYGON_ATTR
        case 0x2A: texImageParamCmd(entry.param); break; // TEXIMAGE_PARAM
        case 0x2B: plttBaseCmd(entry.param);      break; // PLTT_BASE
        case 0x30: difAmbCmd(entry.param);        break; // DIF_AMB
        case 0x31: speEmiCmd(entry.param);        break; // SPE_EMI
        case 0x32: lightVectorCmd(entry.param);   break; // LIGHT_VECTOR
        case 0x33: lightColorCmd(entry.param);    break; // LIGHT_COLOR
        case 0x34: shininessCmd(params);          break; // SHININESS
        case 0x40: beginVtxsCmd(entry.param);     break; // BEGIN_VTXS
        case 0x41:                                break; // END_VTXS
        case 0x50: swapBuffersCmd(entry.param);   break; // SWAP_BUFFERS
        case 0x60: viewportCmd(entry.param);      break; // VIEWPORT
        case 0x70: boxTestCmd(params);            break; // BOX_TEST
        case 0x71: posTestCmd(params);            break; // POS_TEST
        case 0x72: vecTestCmd(entry.param);       break; // VEC_TEST

        default:
        {
            LOG("Unknown GXFIFO command: 0x%X\n", entry.command);
            break;
        }
    }

    // On hardware, FIFO entries are moved into a pipe before being executed
    // The pipe can hold 4 entries, and is refilled when it runs half empty (2 entries)
    // As long as there are enough entries, set the pipe size to 3 or 4 depending on when it would refill
    pipeSize = 4 - ((pipeSize + count) & 1);
    if (pipeSize > fifo.size()) pipeSize = fifo.size();

    // Update the FIFO status
    gxStat = (gxStat & ~0x00001F00) | (coordinatePtr            <<  8); // Coordinate stack pointer
    gxStat = (gxStat & ~0x00002000) | (projectionPtr            << 13); // Projection stack pointer
    gxStat = (gxStat & ~0x01FF0000) | ((fifo.size() - pipeSize) << 16); // FIFO entries
    if (fifo.size() - pipeSize == 0) gxStat |=  BIT(26); // FIFO empty
    if (fifo.size() == 0)            gxStat &= ~BIT(27); // Commands not executing

    // If the FIFO becomes less than half full, trigger GXFIFO DMA transfers
    // If the FIFO is already less than half full when a DMA starts, it will automatically activate
    if (fifo.size() - pipeSize < 128 && !(gxStat & BIT(25)))
    {
        gxStat |= BIT(25);
        core->dma[0].trigger(7);
    }

    // Send a GXFIFO interrupt if enabled
    switch ((gxStat & 0xC0000000) >> 30)
    {
        case 1: if (gxStat & BIT(25)) core->interpreter[0].sendInterrupt(21); break;
        case 2: if (gxStat & BIT(26)) core->interpreter[0].sendInterrupt(21); break;
    }

    // Unhalt the CPU if the FIFO was full but now has space free
    if (fifo.size() - pipeSize <= 256)
        core->interpreter[0].unhalt(1);

    // Keep executing commands as long as they're ready
    if (state != GX_HALTED)
    {
        if (!fifo.empty() && fifo.size() >= paramCounts[fifo.front().command])
            core->schedule(Task(&runCommandTask, 2));
        else
            state = GX_IDLE;
    }
}

void Gpu3D::swapBuffers()
{
    // Normalize and scale the vertices to the viewport
    // X coordinates are 9-bit and Y coordinates are 8-bit; invalid viewports can cause wraparound
    // Z coordinates (and depth values in general) are 24-bit
    for (int i = 0; i < vertexCountIn; i++)
    {
        if (verticesIn[i].w != 0)
        {
            verticesIn[i].x = (( (int64_t)verticesIn[i].x + verticesIn[i].w) * viewportWidth  / (verticesIn[i].w * 2) + viewportX) & 0x1FF;
            verticesIn[i].y = ((-(int64_t)verticesIn[i].y + verticesIn[i].w) * viewportHeight / (verticesIn[i].w * 2) + viewportY) &  0xFF;
            verticesIn[i].z = (((((int64_t)verticesIn[i].z << 14) / verticesIn[i].w) + 0x3FFF) << 9);
        }
    }

    // Determine each polygon's W-shift value to be used for reducing (or expanding) W values to 16 bits
    for (int i = 0; i < polygonCountIn; i++)
    {
        _Polygon *p = &polygonsIn[i];

        // Find the polygon's greatest W value
        uint32_t value = p->vertices[0].w;
        for (int j = 1; j < p->size; j++)
        {
            if ((uint32_t)p->vertices[j].w > value)
                value = p->vertices[j].w;
        }

        // Reduce precision in 4-bit increments until the value fits in a 16-bit range
        while ((value >> p->wShift) > 0xFFFF)
            p->wShift += 4;

        // If precision wasn't reduced, increase it until the value doesn't fit in a 16-bit range
        if (p->wShift == 0 && value != 0)
        {
            while ((value << -(p->wShift - 4)) <= 0xFFFF)
                p->wShift -= 4;
        }
    }

    // Swap the vertex buffers
    SWAP(verticesOut, verticesIn);
    vertexCountOut = vertexCountIn;
    vertexCountIn = 0;
    vertexCount = 0;

    // Swap the polygon buffers
    SWAP(polygonsOut, polygonsIn);
    polygonCountOut = polygonCountIn;
    polygonCountIn = 0;

    // Invalidate the 3D so a new frame is drawn
    core->gpu.invalidate3D();

    // Unhalt the GXFIFO, and start executing commands if one is ready
    if (!fifo.empty() && fifo.size() >= paramCounts[fifo.front().command])
    {
        core->schedule(Task(&runCommandTask, 2));
        state = GX_RUNNING;
    }
    else
    {
        state = GX_IDLE;
    }
}

void Gpu3D::addVertex()
{
    if (vertexCountIn >= 6144) return;

    // Set the new vertex
    verticesIn[vertexCountIn] = savedVertex;
    verticesIn[vertexCountIn].w = 1 << 12;

    // Transform the texture coordinates
    if (textureCoordMode == 3)
    {
        // Get the texture matrix with the texture coordinates
        Matrix matrix = texture;
        matrix.data[12] = (int32_t)s << 12;
        matrix.data[13] = (int32_t)t << 12;

        // Multiply the vertex with the texture matrix
        Vertex vertex = verticesIn[vertexCountIn] * matrix;

        // Save the transformed coordinates
        verticesIn[vertexCountIn].s = vertex.x >> 12;
        verticesIn[vertexCountIn].t = vertex.y >> 12;
    }

    // Update the clip matrix if necessary
    if (clipDirty)
    {
        clip = coordinate * projection;
        clipDirty = false;
    }

    // Transform the vertex
    verticesIn[vertexCountIn] = verticesIn[vertexCountIn] * clip;

    // Move to the next vertex
    vertexCountIn++;
    vertexCount++;

    // Move to the next polygon if one has been completed
    switch (polygonType)
    {
        case 0: if (vertexCount % 3 == 0)                     addPolygon(); break; // Separate triangles
        case 1: if (vertexCount % 4 == 0)                     addPolygon(); break; // Separate quads
        case 2: if (vertexCount >= 3)                         addPolygon(); break; // Triangle strips
        case 3: if (vertexCount >= 4 && vertexCount % 2 == 0) addPolygon(); break; // Quad strips
    }
}

void Gpu3D::addPolygon()
{
    if (polygonCountIn >= 2048) return;

    // Set the polygon vertex information
    int size = 3 + (polygonType & 1);
    savedPolygon.size = size;
    savedPolygon.vertices = &verticesIn[vertexCountIn - size];

    // Save a copy of the unclipped vertices
    Vertex unclipped[4];
    memcpy(unclipped, savedPolygon.vertices, size * sizeof(Vertex));

    // Rearrange quad strip vertices to be counter-clockwise
    if (polygonType == 3)
        SWAP(unclipped[2], unclipped[3]);

    // Get the components of the second and third vertices, relative to the first
    int64_t x1 = unclipped[1].x - unclipped[0].x;
    int64_t y1 = unclipped[1].y - unclipped[0].y;
    int64_t w1 = unclipped[1].w - unclipped[0].w;
    int64_t x2 = unclipped[2].x - unclipped[0].x;
    int64_t y2 = unclipped[2].y - unclipped[0].y;
    int64_t w2 = unclipped[2].w - unclipped[0].w;

    // Calculate the 3D cross product of the vertices
    int64_t xc = y1 * w2 - w1 * y2;
    int64_t yc = w1 * x2 - x1 * w2;
    int64_t wc = x1 * y2 - y1 * x2;

    // Reduce the result to 32 bits to avoid overflow
    while (xc != (int32_t)xc || yc != (int32_t)yc || wc != (int32_t)wc)
    {
        xc >>= 4;
        yc >>= 4;
        wc >>= 4;
    }

    // Calculate the dot product of the cross product and the first vertex to determine 2D orientation
    int64_t dot = xc * unclipped[0].x + yc * unclipped[0].y + wc * unclipped[0].w;

    // Separate from vertex store order, the renderer needs to know the raw polygon orientation to calculate edges
    savedPolygon.clockwise = (dot < 0);

    // Every other triangle strip polygon is stored clockwise instead of counter-clockwise
    // Keep track of this, and reverse the dot product of clockwise polygons to accomodate
    if (polygonType == 2)
    {
        if (clockwise) dot = -dot;
        clockwise = !clockwise;
    }

    // Check if the polygon should be culled, and clip it if not
    Vertex clipped[10];
    bool cull = (!renderFront && dot > 0) || (!renderBack && dot < 0);
    bool clip = cull ? false : clipPolygon(unclipped, clipped, &savedPolygon.size);

    // Discard polygons that should be culled or are outside of the view area
    if (cull || savedPolygon.size == 0)
    {
        switch (polygonType)
        {
            case 0: case 1: // Separate polygons
            {
                // Discard the vertices
                vertexCountIn -= size;
                return;
            }

            case 2: // Triangle strips
            {
                if (vertexCount == 3) // First triangle in the strip
                {
                    // Discard the first vertex, but keep the other 2 for the next triangle
                    verticesIn[vertexCountIn - 3] = verticesIn[vertexCountIn - 2];
                    verticesIn[vertexCountIn - 2] = verticesIn[vertexCountIn - 1];
                    vertexCountIn--;
                    vertexCount--;
                }
                else if (vertexCountIn < 6144)
                {
                    // End the previous strip, and start a new one with the last 2 vertices
                    verticesIn[vertexCountIn - 0] = verticesIn[vertexCountIn - 1];
                    verticesIn[vertexCountIn - 1] = verticesIn[vertexCountIn - 2];
                    vertexCountIn++;
                    vertexCount = 2;
                }
                return;
            }

            case 3: // Quad strips
            {
                if (vertexCount == 4) // First quad in the strip
                {
                    // Discard the first 2 vertices, but keep the other 2 for the next quad
                    verticesIn[vertexCountIn - 4] = verticesIn[vertexCountIn - 2];
                    verticesIn[vertexCountIn - 3] = verticesIn[vertexCountIn - 1];
                    vertexCountIn -= 2;
                    vertexCount -= 2;
                }
                else
                {
                    // End the previous strip, and start a new one with the last 2 vertices
                    vertexCount = 2;
                }
                return;
            }
        }
    }

    // Update the vertices of clipped polygons
    if (clip)
    {
        switch (polygonType)
        {
            case 0: case 1: // Separate polygons
            {
                // Remove the unclipped vertices
                vertexCountIn -= size;

                // Add the clipped vertices
                for (int i = 0; i < savedPolygon.size; i++)
                {
                    if (vertexCountIn >= 6144) return;
                    verticesIn[vertexCountIn] = clipped[i];
                    vertexCountIn++;
                }
                break;
            }

            case 2: // Triangle strips
            {
                // Remove the unclipped vertices
                vertexCountIn -= (vertexCount == 3) ? 3 : 1;
                savedPolygon.vertices = &verticesIn[vertexCountIn];

                // Add the clipped vertices
                for (int i = 0; i < savedPolygon.size; i++)
                {
                    if (vertexCountIn >= 6144) return;
                    verticesIn[vertexCountIn] = clipped[i];
                    vertexCountIn++;
                }

                // End the previous strip, and start a new one with the last 2 vertices
                for (int i = 0; i < 2; i++)
                {
                    if (vertexCountIn >= 6144) return;
                    verticesIn[vertexCountIn] = unclipped[1 + i];
                    vertexCountIn++;
                }
                vertexCount = 2;
                break;
            }

            case 3: // Quad strips
            {
                // Remove the unclipped vertices
                vertexCountIn -= (vertexCount == 4) ? 4 : 2;
                savedPolygon.vertices = &verticesIn[vertexCountIn];

                // Add the clipped vertices
                for (int i = 0; i < savedPolygon.size; i++)
                {
                    if (vertexCountIn >= 6144) return;
                    verticesIn[vertexCountIn] = clipped[i];
                    vertexCountIn++;
                }

                // End the previous strip, and start a new one with the last 2 vertices
                for (int i = 0; i < 2; i++)
                {
                    if (vertexCountIn >= 6144) return;
                    verticesIn[vertexCountIn] = unclipped[3 - i];
                    vertexCountIn++;
                }
                vertexCount = 2;
                break;
            }
        }
    }

    // Set the new polygon
    polygonsIn[polygonCountIn] = savedPolygon;
    polygonsIn[polygonCountIn].crossed = (polygonType == 3 && !clip);
    polygonsIn[polygonCountIn].paletteAddr <<= 4 - (savedPolygon.textureFmt == 2);

    // Move to the next polygon
    polygonCountIn++;
}

void Gpu3D::mtxModeCmd(uint32_t param)
{
    // Set the matrix mode
    matrixMode = param & 0x00000003;
}

void Gpu3D::mtxPushCmd()
{
    // Push the current matrix onto a stack
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            if (projectionPtr < 1)
            {
                // Push to the single projection stack slot and increment the pointer
                projectionStack = projection;
                projectionPtr++;
            }
            else
            {
                // Indicate a matrix stack overflow error
                gxStat |= BIT(15);
            }
            break;
        }

        case 1: case 2: // Coordinate and directional stacks
        {
            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (coordinatePtr >= 30) gxStat |= BIT(15);

            // Push to the current coordinate and directional stack slots and increment the pointer
            if (coordinatePtr < 31)
            {
                coordinateStack[coordinatePtr] = coordinate;
                directionStack[coordinatePtr] = direction;
                coordinatePtr++;
            }
            break;
        }

        case 3: // Texture stack
        {
            // Push to the single texture stack slot
            textureStack = texture;
            break;
        }
    }
}

void Gpu3D::mtxPopCmd(uint32_t param)
{
    // Pop a matrix from a stack
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            if (projectionPtr > 0)
            {
                // Pop from the single projection stack slot and decrement the pointer
                projectionPtr--;
                projection = projectionStack;
                clipDirty = true;
            }
            else
            {
                // Indicate a matrix stack underflow error
                gxStat |= BIT(15);
            }
            break;
        }

        case 1: case 2: // Coordinate and directional stacks
        {
            // Calculate the stack address to pop from
            int address = coordinatePtr - (((param & BIT(5)) ? 0xFFFFFFC0 : 0) | (param & 0x0000003F));

            // Indicate a matrix stack underflow or overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (address < 0 || address >= 30) gxStat |= BIT(15);

            // Pop from the current coordinate and directional stack slots and update the pointer
            if (address >= 0 && address < 31)
            {
                coordinate = coordinateStack[address];
                direction = directionStack[address];
                coordinatePtr = address;
                clipDirty = true;
            }
            break;
        }

        case 3: // Texture stack
        {
            // Pop from the single texture stack slot
            texture = textureStack;
            break;
        }
    }
}

void Gpu3D::mtxStoreCmd(uint32_t param)
{
    // Store a matrix to the stack
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            // Store to the single projection stack slot
            projectionStack = projection;
            break;
        }

        case 1: case 2: // Coordinate and directional stacks
        {
            // Get the stack address to store to
            int address = param & 0x0000001F;

            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (address == 31) gxStat |= BIT(15);

            // Store to the current coordinate and directional stack slots
            coordinateStack[address] = coordinate;
            directionStack[address] = direction;
            break;
        }

        case 3: // Texture stack
        {
            // Store to the single texture stack slot
            textureStack = texture;
            break;
        }
    }
}

void Gpu3D::mtxRestoreCmd(uint32_t param)
{
    // Restore a matrix from the stack
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            // Restore from the single projection stack slot
            projection = projectionStack;
            clipDirty = true;
            break;
        }

        case 1: case 2: // Coordinate and directional stacks
        {
            // Get the stack address to store to
            int address = param & 0x0000001F;

            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (address == 31) gxStat |= BIT(15);

            // Restore from the current coordinate and directional stack slots
            coordinate = coordinateStack[address];
            direction = directionStack[address];
            clipDirty = true;
            break;
       }

        case 3: // Texture stack
        {
            // Restore from the single texture stack slot
            texture = textureStack;
            break;
        }
    }
}

void Gpu3D::mtxIdentityCmd()
{
    // Set a matrix to the identity matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = Matrix();
            clipDirty = true;
            break;
        }

        case 1: // Coordinate stack
        {
            coordinate = Matrix();
            clipDirty = true;
            break;
        }

        case 2: // Coordinate and directional stacks
        {
            coordinate = Matrix();
            direction = Matrix();
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = Matrix();
            break;
        }
    }
}

void Gpu3D::mtxLoad44Cmd(std::vector<uint32_t> &params)
{
    // Convert the parameters to a 4x4 matrix
    Matrix matrix = *(Matrix*)&params[0];

    // Set a matrix to the 4x4 matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = matrix;
            clipDirty = true;
            break;
        }

        case 1: // Coordinate stack
        {
            coordinate = matrix;
            clipDirty = true;
            break;
        }

        case 2: // Coordinate and directional stacks
        {
            coordinate = matrix;
            direction = matrix;
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = matrix;
            break;
        }
    }
}

void Gpu3D::mtxLoad43Cmd(std::vector<uint32_t> &params)
{
    // Convert the parameters to a 4x3 matrix
    Matrix matrix;
    for (int i = 0; i < 4; i++)
        memcpy(&matrix.data[i * 4], &params[i * 3], 3 * sizeof(int32_t));

    // Set a matrix to the 4x3 matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = matrix;
            clipDirty = true;
            break;
        }

        case 1: // Coordinate stack
        {
            coordinate = matrix;
            clipDirty = true;
            break;
        }

        case 2: // Coordinate and directional stacks
        {
            coordinate = matrix;
            direction = matrix;
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = matrix;
            break;
        }
    }
}

void Gpu3D::mtxMult44Cmd(std::vector<uint32_t> &params)
{
    // Convert the parameters to a 4x4 matrix
    Matrix matrix = *(Matrix*)&params[0];

    // Multiply a matrix by the 4x4 matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = matrix * projection;
            clipDirty = true;
            break;
        }

        case 1: // Coordinate stack
        {
            coordinate = matrix * coordinate;
            clipDirty = true;
            break;
        }

        case 2: // Coordinate and directional stacks
        {
            coordinate = matrix * coordinate;
            direction = matrix * direction;
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = matrix * texture;
            break;
        }
    }
}

void Gpu3D::mtxMult43Cmd(std::vector<uint32_t> &params)
{
    // Convert the parameters to a 4x3 matrix
    Matrix matrix;
    for (int i = 0; i < 4; i++)
        memcpy(&matrix.data[i * 4], &params[i * 3], 3 * sizeof(int32_t));

    // Multiply a matrix by the 4x3 matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = matrix * projection;
            clipDirty = true;
            break;
        }

        case 1: // Coordinate stack
        {
            coordinate = matrix * coordinate;
            clipDirty = true;
            break;
        }

        case 2: // Coordinate and directional stacks
        {
            coordinate = matrix * coordinate;
            direction = matrix * direction;
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = matrix * texture;
            break;
        }
    }
}

void Gpu3D::mtxMult33Cmd(std::vector<uint32_t> &params)
{
    // Convert the parameters to a 3x3 matrix
    Matrix matrix;
    for (int i = 0; i < 3; i++)
        memcpy(&matrix.data[i * 4], &params[i * 3], 3 * sizeof(int32_t));

    // Multiply a matrix by the 3x3 matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = matrix * projection;
            clipDirty = true;
            break;
        }

        case 1: // Coordinate stack
        {
            coordinate = matrix * coordinate;
            clipDirty = true;
            break;
        }

        case 2: // Coordinate and directional stacks
        {
            coordinate = matrix * coordinate;
            direction = matrix * direction;
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = matrix * texture;
            break;
        }
    }
}

void Gpu3D::mtxScaleCmd(std::vector<uint32_t> &params)
{
    // Convert the parameters to a scale matrix
    Matrix matrix;
    for (int i = 0; i < 3; i++)
        matrix.data[i * 5] = (int32_t)params[i];

    // Multiply a matrix by the scale matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = matrix * projection;
            clipDirty = true;
            break;
        }

        case 1: case 2: // Coordinate stack
        {
            coordinate = matrix * coordinate;
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = matrix * texture;
            break;
        }
    }
}

void Gpu3D::mtxTransCmd(std::vector<uint32_t> &params)
{
    // Convert the parameters to a translation matrix
    Matrix matrix;
    memcpy(&matrix.data[12], &params[0], 3 * sizeof(int32_t));

    // Multiply a matrix by the translation matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
        {
            projection = matrix * projection;
            clipDirty = true;
            break;
        }

        case 1: // Coordinate stack
        {
            coordinate = matrix * coordinate;
            clipDirty = true;
            break;
        }

        case 2: // Coordinate and directional stacks
        {
            coordinate = matrix * coordinate;
            direction = matrix * direction;
            clipDirty = true;
            break;
        }

        case 3: // Texture stack
        {
            texture = matrix * texture;
            break;
        }
    }
}

void Gpu3D::colorCmd(uint32_t param)
{
    // Set the vertex color
    savedVertex.color = rgb5ToRgb6(param);
}

void Gpu3D::normalCmd(uint32_t param)
{
    // Get the normal vector
    Vertex normalVector;
    normalVector.x = ((int16_t)((param & 0x000003FF) <<  6)) >> 3;
    normalVector.y = ((int16_t)((param & 0x000FFC00) >>  4)) >> 3;
    normalVector.z = ((int16_t)((param & 0x3FF00000) >> 14)) >> 3;

    // Transform the texture coordinates
    if (textureCoordMode == 2)
    {
        // Get the normal vector as a vertex
        Vertex vertex = normalVector;
        vertex.w = 1 << 12;

        // Get the texture matrix with the texture coordinates
        Matrix matrix = texture;
        matrix.data[12] = s << 12;
        matrix.data[13] = t << 12;

        // Multiply the vertex with the matrix
        vertex = vertex * matrix;

        // Save the transformed coordinates
        savedVertex.s = vertex.x >> 12;
        savedVertex.t = vertex.y >> 12;
    }

    // Multiply the normal vector with the directional matrix
    normalVector = normalVector * direction;

    // Set the base vertex color
    savedVertex.color = emissionColor;

    // Calculate the vertex color
    // This is a translation of the pseudocode from GBATEK to C++
    for (int i = 0; i < 4; i++)
    {
        if (enabledLights & BIT(i))
        {
            int diffuseLevel = -(lightVector[i] * normalVector);
            if (diffuseLevel < (0 << 12)) diffuseLevel = (0 << 12);
            if (diffuseLevel > (1 << 12)) diffuseLevel = (1 << 12);

            int shininessLevel = -(halfVector[i] * normalVector);
            if (shininessLevel < (0 << 12)) shininessLevel = (0 << 12);
            if (shininessLevel > (1 << 12)) shininessLevel = (1 << 12);
            shininessLevel = (shininessLevel * shininessLevel) >> 12;

            if (shininessEnabled) shininessLevel = shininess[shininessLevel >> 5] << 4;

            int r = (savedVertex.color >>  0) & 0x3F;
            int g = (savedVertex.color >>  6) & 0x3F;
            int b = (savedVertex.color >> 12) & 0x3F;

            r += (((specularColor >>  0) & 0x3F) * ((lightColor[i] >>  0) & 0x3F) * shininessLevel) >> 18;
            g += (((specularColor >>  6) & 0x3F) * ((lightColor[i] >>  6) & 0x3F) * shininessLevel) >> 18;
            b += (((specularColor >> 12) & 0x3F) * ((lightColor[i] >> 12) & 0x3F) * shininessLevel) >> 18;

            r += (((diffuseColor >>  0) & 0x3F) * ((lightColor[i] >>  0) & 0x3F) * diffuseLevel) >> 18;
            g += (((diffuseColor >>  6) & 0x3F) * ((lightColor[i] >>  6) & 0x3F) * diffuseLevel) >> 18;
            b += (((diffuseColor >> 12) & 0x3F) * ((lightColor[i] >> 12) & 0x3F) * diffuseLevel) >> 18;

            r += ((ambientColor >>  0) & 0x3F) * ((lightColor[i] >>  0) & 0x3F) >> 6;
            g += ((ambientColor >>  6) & 0x3F) * ((lightColor[i] >>  6) & 0x3F) >> 6;
            b += ((ambientColor >> 12) & 0x3F) * ((lightColor[i] >> 12) & 0x3F) >> 6;

            if (r < 0) r = 0; if (r > 0x3F) r = 0x3F;
            if (g < 0) g = 0; if (g > 0x3F) g = 0x3F;
            if (b < 0) b = 0; if (b > 0x3F) b = 0x3F;

            savedVertex.color = (b << 12) | (g << 6) | r;
        }
    }
}

void Gpu3D::texCoordCmd(uint32_t param)
{
    // Set the untransformed texture coordinates
    s = (int16_t)(param >>  0);
    t = (int16_t)(param >> 16);

    // Transform the texture coordinates
    if (textureCoordMode == 1)
    {
        // Create a vertex with the texture coordinates
        Vertex vertex;
        vertex.x = s << 8;
        vertex.y = t << 8;
        vertex.z = 1 << 8;
        vertex.w = 1 << 8;

        // Multiply the vertex with the texture matrix
        vertex = vertex * texture;

        // Save the transformed coordinates
        savedVertex.s = vertex.x >> 8;
        savedVertex.t = vertex.y >> 8;
    }
    else
    {
        savedVertex.s = s;
        savedVertex.t = t;
    }
}

void Gpu3D::vtx16Cmd(std::vector<uint32_t> &params)
{
    // Set the X, Y, and Z coordinates
    savedVertex.x = (int16_t)(params[0] >>  0);
    savedVertex.y = (int16_t)(params[0] >> 16);
    savedVertex.z = (int16_t)(params[1]);

    addVertex();
}

void Gpu3D::vtx10Cmd(uint32_t param)
{
    // Set the X, Y, and Z coordinates
    savedVertex.x = (int16_t)((param & 0x000003FF) << 6);
    savedVertex.y = (int16_t)((param & 0x000FFC00) >> 4);
    savedVertex.z = (int16_t)((param & 0x3FF00000) >> 14);

    addVertex();
}

void Gpu3D::vtxXYCmd(uint32_t param)
{
    // Set the X and Y coordinates
    savedVertex.x = (int16_t)(param >>  0);
    savedVertex.y = (int16_t)(param >> 16);

    addVertex();
}

void Gpu3D::vtxXZCmd(uint32_t param)
{
    // Set the X and Z coordinates
    savedVertex.x = (int16_t)(param >>  0);
    savedVertex.z = (int16_t)(param >> 16);

    addVertex();
}

void Gpu3D::vtxYZCmd(uint32_t param)
{
    // Set the Y and Z coordinates
    savedVertex.y = (int16_t)(param >>  0);
    savedVertex.z = (int16_t)(param >> 16);

    addVertex();
}

void Gpu3D::vtxDiffCmd(uint32_t param)
{
    // Add offsets to the X, Y, and Z coordinates
    savedVertex.x += ((int16_t)((param & 0x000003FF) <<  6) / 8) >> 3;
    savedVertex.y += ((int16_t)((param & 0x000FFC00) >>  4) / 8) >> 3;
    savedVertex.z += ((int16_t)((param & 0x3FF00000) >> 14) / 8) >> 3;

    addVertex();
}

void Gpu3D::polygonAttrCmd(uint32_t param)
{
    // Set the polygon attributes
    // Values are not actually applied until the next vertex list
    polygonAttr = param;
}

void Gpu3D::texImageParamCmd(uint32_t param)
{
    // Set the texture parameters
    savedPolygon.textureAddr = (param & 0x0000FFFF) << 3;
    savedPolygon.sizeS = 8 << ((param & 0x00700000) >> 20);
    savedPolygon.sizeT = 8 << ((param & 0x03800000) >> 23);
    savedPolygon.repeatS = param & BIT(16);
    savedPolygon.repeatT = param & BIT(17);
    savedPolygon.flipS = param & BIT(18);
    savedPolygon.flipT = param & BIT(19);
    savedPolygon.textureFmt = (param & 0x1C000000) >> 26;
    savedPolygon.transparent0 = param & BIT(29);
    textureCoordMode = (param & 0xC0000000) >> 30;
}

void Gpu3D::plttBaseCmd(uint32_t param)
{
    // Set the palette base address
    savedPolygon.paletteAddr = param & 0x00001FFF;
}

void Gpu3D::difAmbCmd(uint32_t param)
{
    // Set the diffuse and ambient reflection colors
    diffuseColor = rgb5ToRgb6(param >>  0);
    ambientColor = rgb5ToRgb6(param >> 16);

    // Directly set the vertex color
    if (param & BIT(15))
        savedVertex.color = diffuseColor;
}

void Gpu3D::speEmiCmd(uint32_t param)
{
    // Set the specular reflection and emission colors
    specularColor = rgb5ToRgb6(param >>  0);
    emissionColor = rgb5ToRgb6(param >> 16);

    // Set the shininess table toggle
    shininessEnabled = param & BIT(15);
}

void Gpu3D::lightVectorCmd(uint32_t param)
{
    // Set one of the light vectors
    lightVector[param >> 30].x = ((int16_t)((param & 0x000003FF) <<  6)) >> 3;
    lightVector[param >> 30].y = ((int16_t)((param & 0x000FFC00) >>  4)) >> 3;
    lightVector[param >> 30].z = ((int16_t)((param & 0x3FF00000) >> 14)) >> 3;

    // Multiply the light vector by the directional matrix
    lightVector[param >> 30] = lightVector[param >> 30] * direction;

    // Set one of the half vectors
    halfVector[param >> 30].x = (lightVector[param >> 30].x)             / 2;
    halfVector[param >> 30].y = (lightVector[param >> 30].y)             / 2;
    halfVector[param >> 30].z = (lightVector[param >> 30].z - (1 << 12)) / 2;
}

void Gpu3D::lightColorCmd(uint32_t param)
{
    // Set one of the light colors
    lightColor[param >> 30] = rgb5ToRgb6(param);
}

void Gpu3D::shininessCmd(std::vector<uint32_t> &params)
{
    // Set the values of the specular reflection shininess table
    for (int i = 0; i < 32; i++)
    {
        shininess[i * 4 + 0] = params[i] >>  0;
        shininess[i * 4 + 1] = params[i] >>  8;
        shininess[i * 4 + 2] = params[i] >> 16;
        shininess[i * 4 + 3] = params[i] >> 24;
    }
}

void Gpu3D::beginVtxsCmd(uint32_t param)
{
    // Clipping a polygon strip starts a new strip with the last 2 vertices of the old one
    // Discard these vertices if they're unused
    if (vertexCount < 3 + (polygonType & 1))
        vertexCountIn -= vertexCount;

    // Begin a new vertex list
    polygonType = param & 0x00000003;
    vertexCount = 0;
    clockwise = false;

    // Apply the polygon attributes
    enabledLights = polygonAttr & 0x0000000F;
    savedPolygon.mode = (polygonAttr & 0x00000030) >> 4;
    renderBack = polygonAttr & BIT(6);
    renderFront = polygonAttr & BIT(7);
    savedPolygon.transNewDepth = polygonAttr & BIT(11);
    savedPolygon.depthTestEqual = polygonAttr & BIT(14);
    savedPolygon.fog = polygonAttr & BIT(15);
    savedPolygon.alpha = ((polygonAttr & 0x001F0000) >> 16) * 2;
    if (savedPolygon.alpha > 0) savedPolygon.alpha++;
    savedPolygon.id = (polygonAttr & 0x3F000000) >> 24;
}

void Gpu3D::swapBuffersCmd(uint32_t param)
{
    // Set the W-buffering toggle
    savedPolygon.wBuffer = param & BIT(1);

    // Halt the geometry engine
    // The buffers will be swapped and the engine unhalted on next V-blank
    state = GX_HALTED;
}

void Gpu3D::viewportCmd(uint32_t param)
{
    // Set the viewport dimensions
    viewportX      =         ((param & 0x000000FF) >>  0)                   & 0x1FF;
    viewportY      =  (191 - ((param & 0xFF000000) >> 24))                  &  0xFF;
    viewportWidth  =        (((param & 0x00FF0000) >> 16)  - viewportX + 1) & 0x1FF;
    viewportHeight = ((191 - ((param & 0x0000FF00) >>  8)) - viewportY + 1) &  0xFF;
}

void Gpu3D::boxTestCmd(std::vector<uint32_t> &params)
{
    // Store the parameters (X-pos, Y-pos, Z-pos, width, height, depth)
    int16_t boxTestCoords[6] =
    {
        (int16_t)params[0], (int16_t)(params[0] >> 16),
        (int16_t)params[1], (int16_t)(params[1] >> 16),
        (int16_t)params[2], (int16_t)(params[2] >> 16)
    };

    // Get the vertices of the box
    Vertex vertices[8];
    vertices[0].x = boxTestCoords[0];
    vertices[0].y = boxTestCoords[1];
    vertices[0].z = boxTestCoords[2];
    vertices[1].x = boxTestCoords[0] + boxTestCoords[3];
    vertices[1].y = boxTestCoords[1];
    vertices[1].z = boxTestCoords[2];
    vertices[2].x = boxTestCoords[0];
    vertices[2].y = boxTestCoords[1] + boxTestCoords[4];
    vertices[2].z = boxTestCoords[2];
    vertices[3].x = boxTestCoords[0];
    vertices[3].y = boxTestCoords[1];
    vertices[3].z = boxTestCoords[2] + boxTestCoords[5];
    vertices[4].x = boxTestCoords[0] + boxTestCoords[3];
    vertices[4].y = boxTestCoords[1] + boxTestCoords[4];
    vertices[4].z = boxTestCoords[2];
    vertices[5].x = boxTestCoords[0] + boxTestCoords[3];
    vertices[5].y = boxTestCoords[1];
    vertices[5].z = boxTestCoords[2] + boxTestCoords[5];
    vertices[6].x = boxTestCoords[0];
    vertices[6].y = boxTestCoords[1] + boxTestCoords[4];
    vertices[6].z = boxTestCoords[2] + boxTestCoords[5];
    vertices[7].x = boxTestCoords[0] + boxTestCoords[3];
    vertices[7].y = boxTestCoords[1] + boxTestCoords[4];
    vertices[7].z = boxTestCoords[2] + boxTestCoords[5];

    // Update the clip matrix if necessary
    if (clipDirty)
    {
        clip = coordinate * projection;
        clipDirty = false;
    }

    // Transform the vertices
    for (int i = 0; i < 8; i++)
    {
        vertices[i].w = 1 << 12;
        vertices[i] = vertices[i] * clip;
    }

    // Arrange the vertices to represent the faces of the box
    Vertex faces[6][4] =
    {
        { vertices[0], vertices[1], vertices[4], vertices[2] },
        { vertices[3], vertices[5], vertices[7], vertices[6] },
        { vertices[3], vertices[5], vertices[1], vertices[0] },
        { vertices[6], vertices[7], vertices[4], vertices[2] },
        { vertices[0], vertices[3], vertices[6], vertices[2] },
        { vertices[1], vertices[5], vertices[7], vertices[4] }
    };

    // Clip the faces of the box
    // If any of the faces are in view, set the result bit
    for (int i = 0; i < 6; i++)
    {
        int size = 4;
        Vertex clipped[10];
        clipPolygon(faces[i], clipped, &size);

        if (size > 0)
        {
            gxStat |= BIT(1);
            return;
        }
    }

    // Clear the result bit if none of the faces were in view
    gxStat &= ~BIT(1);
}

void Gpu3D::posTestCmd(std::vector<uint32_t> &params)
{
    // Set the X, Y, and Z coordinates, overwriting the saved vertex
    savedVertex.x = (int16_t)(params[0] >>  0);
    savedVertex.y = (int16_t)(params[0] >> 16);
    savedVertex.z = (int16_t)(params[1]);
    savedVertex.w = 1 << 12;

    // Update the clip matrix if necessary
    if (clipDirty)
    {
        clip = coordinate * projection;
        clipDirty = false;
    }

    // Multiply the vertex with the clip matrix and set the result
    Vertex vertex = savedVertex * clip;
    posResult[0] = vertex.x;
    posResult[1] = vertex.y;
    posResult[2] = vertex.z;
    posResult[3] = vertex.w;
}

void Gpu3D::vecTestCmd(uint32_t param)
{
    // Set the vector components
    Vertex vector;
    vector.x = ((int16_t)((param & 0x000003FF) <<  6)) >> 3;
    vector.y = ((int16_t)((param & 0x000FFC00) >>  4)) >> 3;
    vector.z = ((int16_t)((param & 0x3FF00000) >> 14)) >> 3;

    // Multiply the vector with the directional matrix and set the result
    // The result has 4-bit signs and 12-bit fractions
    vector = vector * direction;
    vecResult[0] = ((int16_t)(vector.x << 3)) >> 3;
    vecResult[1] = ((int16_t)(vector.y << 3)) >> 3;
    vecResult[2] = ((int16_t)(vector.z << 3)) >> 3;
}

void Gpu3D::addEntry(Entry entry)
{
    if (fifo.size() - pipeSize == 0 && pipeSize < 4)
    {
        // Move data directly into the pipe if the FIFO is empty and the pipe isn't full
        fifo.push(entry);
        pipeSize++;

        // Update the FIFO status
        gxStat |= BIT(27); // Commands executing
    }
    else
    {
        // If the FIFO is full, halt the CPU until space is free
        if (fifo.size() - pipeSize >= 256)
            core->interpreter[0].halt(1);

        // Move data into the FIFO
        fifo.push(entry);

        // Update the FIFO status
        gxStat = (gxStat & ~0x01FF0000) | ((fifo.size() - pipeSize) << 16); // FIFO entries
        gxStat &= ~BIT(26); // FIFO not empty

        // If the FIFO is half full or more, disable GXFIFO DMA transfers
        if (fifo.size() - pipeSize >= 128 && (gxStat & BIT(25)))
            gxStat &= ~BIT(25);
    }

    // Start executing commands if one is ready
    if (state == GX_IDLE && fifo.size() >= paramCounts[fifo.front().command])
    {
        core->schedule(Task(&runCommandTask, 2));
        state = GX_RUNNING;
    }
}

void Gpu3D::writeGxFifo(uint32_t mask, uint32_t value)
{
    if (gxFifo == 0)
    {
        // Read new packed commands
        gxFifo = value & mask;
    }
    else
    {
        // Add a command parameter
        Entry entry(gxFifo, value & mask);
        addEntry(entry);
        gxFifoCount++;

        // Move to the next command once all parameters have been sent
        if (gxFifoCount == paramCounts[gxFifo & 0xFF])
        {
            gxFifo >>= 8;
            gxFifoCount = 0;
        }
    }

    // Add entries for commands with no parameters
    while (gxFifo != 0 && paramCounts[gxFifo & 0xFF] == 0)
    {
        Entry entry(gxFifo, 0);
        addEntry(entry);
        gxFifo >>= 8;
    }
}

void Gpu3D::writeMtxMode(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x10, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxPush(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x11, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxPop(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x12, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxStore(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x13, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxRestore(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x14, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxIdentity(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x15, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxLoad44(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x16, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxLoad43(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x17, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxMult44(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x18, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxMult43(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x19, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxMult33(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x1A, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxScale(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x1B, value & mask);
    addEntry(entry);
}

void Gpu3D::writeMtxTrans(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x1C, value & mask);
    addEntry(entry);
}

void Gpu3D::writeColor(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x20, value & mask);
    addEntry(entry);
}

void Gpu3D::writeNormal(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x21, value & mask);
    addEntry(entry);
}

void Gpu3D::writeTexCoord(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x22, value & mask);
    addEntry(entry);
}

void Gpu3D::writeVtx16(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x23, value & mask);
    addEntry(entry);
}

void Gpu3D::writeVtx10(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x24, value & mask);
    addEntry(entry);
}

void Gpu3D::writeVtxXY(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x25, value & mask);
    addEntry(entry);
}

void Gpu3D::writeVtxXZ(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x26, value & mask);
    addEntry(entry);
}

void Gpu3D::writeVtxYZ(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x27, value & mask);
    addEntry(entry);
}

void Gpu3D::writeVtxDiff(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x28, value & mask);
    addEntry(entry);
}

void Gpu3D::writePolygonAttr(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x29, value & mask);
    addEntry(entry);
}

void Gpu3D::writeTexImageParam(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x2A, value & mask);
    addEntry(entry);
}

void Gpu3D::writePlttBase(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x2B, value & mask);
    addEntry(entry);
}

void Gpu3D::writeDifAmb(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x30, value & mask);
    addEntry(entry);
}

void Gpu3D::writeSpeEmi(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x31, value & mask);
    addEntry(entry);
}

void Gpu3D::writeLightVector(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x32, value & mask);
    addEntry(entry);
}

void Gpu3D::writeLightColor(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x33, value & mask);
    addEntry(entry);
}

void Gpu3D::writeShininess(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x34, value & mask);
    addEntry(entry);
}

void Gpu3D::writeBeginVtxs(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x40, value & mask);
    addEntry(entry);
}

void Gpu3D::writeEndVtxs(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x41, value & mask);
    addEntry(entry);
}

void Gpu3D::writeSwapBuffers(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x50, value & mask);
    addEntry(entry);
}

void Gpu3D::writeViewport(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x60, value & mask);
    addEntry(entry);
}

void Gpu3D::writeBoxTest(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x70, value & mask);
    addEntry(entry);
}

void Gpu3D::writePosTest(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x71, value & mask);
    addEntry(entry);
}

void Gpu3D::writeVecTest(uint32_t mask, uint32_t value)
{
    // Add an entry to the FIFO
    Entry entry(0x72, value & mask);
    addEntry(entry);
}

void Gpu3D::writeGxStat(uint32_t mask, uint32_t value)
{
    // Clear the error bit and reset the projection stack pointer
    if (value & BIT(15))
    {
        gxStat &= ~0x0000A000;
        projectionPtr = 0;
    }

    // Write to the GXSTAT register
    mask &= 0xC0000000;
    gxStat = (gxStat & ~mask) | (value & mask);
}

uint32_t Gpu3D::readRamCount()
{
    // Read from the RAM_COUNT register
    return (vertexCountIn << 16) | polygonCountIn;
}

uint32_t Gpu3D::readClipMtxResult(int index)
{
    // Update the clip matrix if necessary
    if (clipDirty)
    {
        clip = coordinate * projection;
        clipDirty = false;
    }

    // Read from one of the CLIPMTX_RESULT registers
    return clip.data[index];
}

uint32_t Gpu3D::readVecMtxResult(int index)
{
    // Read from one of the VECMTX_RESULT registers
    return direction.data[(index / 3) * 4 + index % 3];
}
