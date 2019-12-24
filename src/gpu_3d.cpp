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

#include <cstdio>
#include <cstring>

#include "gpu_3d.h"
#include "defines.h"
#include "interpreter.h"

Gpu3D::Gpu3D(Interpreter *arm9): arm9(arm9)
{
    // Set the parameter counts
    paramCounts[0x10] = 1;
    paramCounts[0x11] = 0;
    paramCounts[0x12] = 1;
    paramCounts[0x13] = 1;
    paramCounts[0x14] = 1;
    paramCounts[0x15] = 0;
    paramCounts[0x16] = 16;
    paramCounts[0x17] = 12;
    paramCounts[0x18] = 16;
    paramCounts[0x19] = 12;
    paramCounts[0x1A] = 9;
    paramCounts[0x1B] = 3;
    paramCounts[0x1C] = 3;
    paramCounts[0x20] = 1;
    paramCounts[0x21] = 1;
    paramCounts[0x22] = 1;
    paramCounts[0x23] = 2;
    paramCounts[0x24] = 1;
    paramCounts[0x25] = 1;
    paramCounts[0x26] = 1;
    paramCounts[0x27] = 1;
    paramCounts[0x28] = 1;
    paramCounts[0x29] = 1;
    paramCounts[0x2A] = 1;
    paramCounts[0x2B] = 1;
    paramCounts[0x30] = 1;
    paramCounts[0x31] = 1;
    paramCounts[0x32] = 1;
    paramCounts[0x33] = 1;
    paramCounts[0x34] = 32;
    paramCounts[0x40] = 1;
    paramCounts[0x41] = 0;
    paramCounts[0x50] = 1;
    paramCounts[0x60] = 1;
    paramCounts[0x70] = 3;
    paramCounts[0x71] = 2;
    paramCounts[0x72] = 1;
}

void Gpu3D::runCycle()
{
    // Fetch the next geometry command
    Entry entry = pipe.front();
    pipe.pop();

    // Execute the geometry command
    switch (entry.command)
    {
        case 0x10: mtxModeCmd(entry.param);       break; // MTX_MODE
        case 0x11: mtxPushCmd();                  break; // MTX_PUSH
        case 0x12: mtxPopCmd(entry.param);        break; // MTX_POP
        case 0x13: mtxStoreCmd(entry.param);      break; // MTX_STORE
        case 0x14: mtxRestoreCmd(entry.param);    break; // MTX_RESTORE
        case 0x15: mtxIdentityCmd();              break; // MTX_IDENTITY
        case 0x16: mtxLoad44Cmd(entry.param);     break; // MTX_LOAD_4x4
        case 0x17: mtxLoad43Cmd(entry.param);     break; // MTX_LOAD_4x3
        case 0x18: mtxMult44Cmd(entry.param);     break; // MTX_MULT_4x4
        case 0x19: mtxMult43Cmd(entry.param);     break; // MTX_MULT_4x3
        case 0x1A: mtxMult33Cmd(entry.param);     break; // MTX_MULT_3x3
        case 0x1B: mtxScaleCmd(entry.param);      break; // MTX_SCALE
        case 0x1C: mtxTransCmd(entry.param);      break; // MTX_TRANS
        case 0x20: colorCmd(entry.param);         break; // COLOR
        case 0x22: texCoordCmd(entry.param);      break; // TEXCOORD
        case 0x23: vtx16Cmd(entry.param);         break; // VTX_16
        case 0x24: vtx10Cmd(entry.param);         break; // VTX_10
        case 0x25: vtxXYCmd(entry.param);         break; // VTX_XY
        case 0x26: vtxXZCmd(entry.param);         break; // VTX_XZ
        case 0x27: vtxYZCmd(entry.param);         break; // VTX_YZ
        case 0x28: vtxDiffCmd(entry.param);       break; // VTX_DIFF
        case 0x2A: texImageParamCmd(entry.param); break; // TEXIMAGE_PARAM
        case 0x2B: plttBaseCmd(entry.param);      break; // PLTT_BASE
        case 0x40: beginVtxsCmd(entry.param);     break; // BEGIN_VTXS
        case 0x41:                                break; // END_VTXS
        case 0x50: swapBuffersCmd(entry.param);   break; // SWAP_BUFFERS

        default:
            printf("Unknown GXFIFO command: 0x%X\n", entry.command);
            break;
    }

    // Keep track of how many parameters have been sent
    paramCount++;
    if (paramCount >= paramCounts[entry.command])
        paramCount = 0;

    // Move 2 FIFO entries into the PIPE if it runs half empty
    if (pipe.size() < 3)
    {
        for (int i = 0; i < ((fifo.size() > 2) ? 2 : fifo.size()); i++)
        {
            pipe.push(fifo.front());
            fifo.pop();
        }
    }

    // Update the counters
    gxStat = (gxStat & ~0x00001F00) | (coordPointer << 8); // Coordinate stack pointer
    gxStat = (gxStat & ~0x00002000) | (projPointer << 13); // Projection stack pointer
    gxStat = (gxStat & ~0x01FF0000) | (fifo.size() << 16); // FIFO entries

    // Update the FIFO status
    if (fifo.size() < 128) gxStat |=  BIT(25); // Less than half full
    if (fifo.size() == 0)  gxStat |=  BIT(26); // Empty
    if (pipe.size() == 0)  gxStat &= ~BIT(27); // Commands not executing

    // Send a GXFIFO interrupt if enabled
    switch ((gxStat & 0xC0000000) >> 30)
    {
        case 1: if (gxStat & BIT(25)) arm9->sendInterrupt(21); break;
        case 2: if (gxStat & BIT(26)) arm9->sendInterrupt(21); break;
    }
}

void Gpu3D::swapBuffers()
{
    // Normalize and convert the vertices' X and Y coordinates to DS screen coordinates
    for (int i = 0; i < vertexCountIn; i++)
    {
        if (verticesIn[i].w != 0)
        {
            verticesIn[i].x = (( verticesIn[i].x * 128) / verticesIn[i].w) + 128;
            verticesIn[i].y = ((-verticesIn[i].y *  96) / verticesIn[i].w) +  96;
        }
    }

    // Swap the vertex buffers
    Vertex *vertices = verticesOut;
    verticesOut = verticesIn;
    verticesIn = vertices;
    vertexCountOut = vertexCountIn;
    vertexCountIn = 0;

    // Swap the polygon buffers
    _Polygon *polygons = polygonsOut;
    polygonsOut = polygonsIn;
    polygonsIn = polygons;
    polygonCountOut = polygonCountIn;
    polygonCountIn = 0;

    // Unhalt the geometry engine
    halted = false;
}

Matrix Gpu3D::multiply(Matrix *mtx1, Matrix *mtx2)
{
    Matrix matrix = {{}};

    // Multiply 2 matrices
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            for (int i = 0; i < 4; i++)
                matrix.data[y * 4 + x] += (mtx1->data[y * 4 + i] * mtx2->data[i * 4 + x]) >> 12;
        }
    }

    return matrix;
}

Vertex Gpu3D::multiply(Vertex *vtx, Matrix *mtx)
{
    Vertex vertex = *vtx;

    // Multiply a vertex with a matrix
    vertex.x = (vtx->x * mtx->data[0] + vtx->y * mtx->data[4] + vtx->z * mtx->data[8]  + vtx->w * mtx->data[12]) >> 12;
    vertex.y = (vtx->x * mtx->data[1] + vtx->y * mtx->data[5] + vtx->z * mtx->data[9]  + vtx->w * mtx->data[13]) >> 12;
    vertex.z = (vtx->x * mtx->data[2] + vtx->y * mtx->data[6] + vtx->z * mtx->data[10] + vtx->w * mtx->data[14]) >> 12;
    vertex.w = (vtx->x * mtx->data[3] + vtx->y * mtx->data[7] + vtx->z * mtx->data[11] + vtx->w * mtx->data[15]) >> 12;

    return vertex;
}

void Gpu3D::addVertex()
{
    // Remember the pre-transformation vertex
    last = verticesIn[vertexCountIn];

    // Update the clip matrix if necessary
    if (clipNeedsUpdate)
    {
        clip = multiply(&coordinate, &projection);
        clipNeedsUpdate = false;
    }

    // Transform the vertex
    verticesIn[vertexCountIn] = multiply(&verticesIn[vertexCountIn], &clip);

    // Set the vertex color (converted from RGB5 to RGB6)
    uint8_t r = ((savedColor >>  0) & 0x1F); r = r * 2 + (r + 31) / 32;
    uint8_t g = ((savedColor >>  5) & 0x1F); g = g * 2 + (g + 31) / 32;
    uint8_t b = ((savedColor >> 10) & 0x1F); b = b * 2 + (b + 31) / 32;
    verticesIn[vertexCountIn].color = (b << 12) | (g << 6) | r;

    // Set the vertex texture coordinates
    verticesIn[vertexCountIn].s = (int16_t)(savedTexCoord & 0x0000FFFF);
    verticesIn[vertexCountIn].t = (int16_t)((savedTexCoord & 0xFFFF0000) >> 16);

    // Move to the next vertex
    vertexCountIn++;
    size++;

    // Move to the next polygon if one has been completed
    if (size > 0)
    {
        switch (savedBeginVtxs)
        {
            case 0: if (size % 3 == 0)              addPolygon(); break; // Separate triangles
            case 1: if (size % 4 == 0)              addPolygon(); break; // Separate quads
            case 2: if (size >= 3)                  addPolygon(); break; // Triangle strips
            case 3: if (size >= 4 && size % 2 == 0) addPolygon(); break; // Quad strips
        }
    }
}

void Gpu3D::addPolygon()
{
    if (polygonCountIn >= 2048) return;

    // Set the polygon parameters
    int vertexCount = 3 + (savedBeginVtxs & 1);
    polygonsIn[polygonCountIn].size = vertexCount;
    polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn - vertexCount];

    Vertex unclipped[8];
    Vertex clipped[8];
    Vertex temp[8];

    // Save a copy of the unclipped vertices
    for (int i = 0; i < vertexCount; i++)
        unclipped[i] = polygonsIn[polygonCountIn].vertices[i];

    // Rearrange quad strip vertices to work with the clipping algorithm
    if (savedBeginVtxs == 3)
    {
        Vertex vertex = unclipped[2];
        unclipped[2] = unclipped[3];
        unclipped[3] = vertex;
    }

    // Clip the polygon on all 6 sides of the view area
    bool clip = clipPolygon(unclipped, temp, 0);
    clip |= clipPolygon(temp, clipped, 1);
    clip |= clipPolygon(clipped, temp, 2);
    clip |= clipPolygon(temp, clipped, 3);
    clip |= clipPolygon(clipped, temp, 4);
    clip |= clipPolygon(temp, clipped, 5);

    // Discard polygons that are completely outside of the view area
    if (polygonsIn[polygonCountIn].size == 0)
    {
        switch (savedBeginVtxs)
        {
            case 0: case 1: // Separate polygons
                // Discard the vertices
                vertexCountIn -= vertexCount;
                return;

            case 2: // Triangle strips
                if (size == 3) // First triangle in the strip
                {
                    // Discard the first vertex, but keep the other 2 for the next triangle
                    verticesIn[vertexCountIn - 3] = verticesIn[vertexCountIn - 2];
                    verticesIn[vertexCountIn - 2] = verticesIn[vertexCountIn - 1];
                    vertexCountIn--;
                    size--;
                }
                else
                {
                    // End the previous strip, and start a new one with the last 2 vertices
                    verticesIn[vertexCountIn - 1] = verticesIn[vertexCountIn - 2];
                    verticesIn[vertexCountIn - 0] = verticesIn[vertexCountIn - 1];
                    vertexCountIn++;
                    size = 2;
                }
                return;

            case 3: // Quad strips
                if (size == 4) // First quad in the strip
                {
                    // Discard the first 2 vertices, but keep the other 2 for the next quad
                    verticesIn[vertexCountIn - 4] = verticesIn[vertexCountIn - 2];
                    verticesIn[vertexCountIn - 3] = verticesIn[vertexCountIn - 1];
                    vertexCountIn -= 2;
                    size -= 2;
                }
                else
                {
                    // End the previous strip, and start a new one with the last 2 vertices
                    size = 2;
                }
                return;
        }
    }

    // Update the vertices of clipped polygons
    if (clip)
    {
        switch (savedBeginVtxs)
        {
            case 0: case 1: // Separate polygons
                // Remove the unclipped vertices
                vertexCountIn -= vertexCount;
                polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn];

                // Add the clipped vertices
                for (int i = 0; i < polygonsIn[polygonCountIn].size; i++)
                {
                    verticesIn[vertexCountIn] = clipped[i];
                    vertexCountIn++;
                }
                break;

            case 2: // Triangle strips
                // Remove the unclipped vertices
                vertexCountIn -= (size == 3) ? 3 : 1;
                polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn];

                // Add the clipped vertices
                for (int i = 0; i < polygonsIn[polygonCountIn].size; i++)
                {
                    verticesIn[vertexCountIn] = clipped[i];
                    vertexCountIn++;
                }

                // End the previous strip, and start a new one with the last 2 vertices
                verticesIn[vertexCountIn + 0] = unclipped[1];
                verticesIn[vertexCountIn + 1] = unclipped[2];
                vertexCountIn += 2;
                size = 2;
                break;

            case 3: // Quad strips
                // Remove the unclipped vertices
                vertexCountIn -= (size == 4) ? 4 : 2;
                polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn];

                // Add the clipped vertices
                for (int i = 0; i < polygonsIn[polygonCountIn].size; i++)
                {
                    verticesIn[vertexCountIn] = clipped[i];
                    vertexCountIn++;
                }

                // End the previous strip, and start a new one with the last 2 vertices
                verticesIn[vertexCountIn + 0] = unclipped[3];
                verticesIn[vertexCountIn + 1] = unclipped[2];
                vertexCountIn += 2;
                size = 2;
                break;
        }
    }

    // Set the texture parameters
    polygonsIn[polygonCountIn].texDataAddr = (savedTexImageParam & 0x0000FFFF) * 8;
    polygonsIn[polygonCountIn].repeatS = savedTexImageParam & BIT(16);
    polygonsIn[polygonCountIn].repeatT = savedTexImageParam & BIT(17);
    polygonsIn[polygonCountIn].flipS = savedTexImageParam & BIT(18);
    polygonsIn[polygonCountIn].flipT = savedTexImageParam & BIT(19);
    polygonsIn[polygonCountIn].sizeS = 8 << ((savedTexImageParam & 0x00700000) >> 20);
    polygonsIn[polygonCountIn].sizeT = 8 << ((savedTexImageParam & 0x03800000) >> 23);
    polygonsIn[polygonCountIn].texFormat = (savedTexImageParam & 0x1C000000) >> 26;
    polygonsIn[polygonCountIn].transparent = savedTexImageParam & BIT(29);
    polygonsIn[polygonCountIn].texPaletteAddr = (savedPlttBase & 0x00001FFF) * ((polygonsIn[polygonCountIn].texFormat == 2) ? 8 : 16);

    // Move to the next polygon
    polygonCountIn++;
}

Vertex Gpu3D::intersection(Vertex *v0, Vertex *v1, int64_t val0, int64_t val1)
{
    Vertex vertex;

    // Calculate the interpolation coefficients
    int64_t d0 = val0 + v0->w;
    int64_t d1 = val1 + v1->w;
    if (d1 == d0) return *v0;

    // Interpolate the vertex coordinates
    vertex.x = ((v0->x * d1) - (v1->x * d0)) / (d1 - d0);
    vertex.y = ((v0->y * d1) - (v1->y * d0)) / (d1 - d0);
    vertex.z = ((v0->z * d1) - (v1->z * d0)) / (d1 - d0);
    vertex.w = ((v0->w * d1) - (v1->w * d0)) / (d1 - d0);
    vertex.s = ((v0->s * d1) - (v1->s * d0)) / (d1 - d0);
    vertex.t = ((v0->t * d1) - (v1->t * d0)) / (d1 - d0);

    // Interpolate the vertex color
    uint8_t r = ((((v0->color >>  0) & 0x3F) * d1) - (((v1->color >>  0) & 0x3F) * d0)) / (d1 - d0);
    uint8_t g = ((((v0->color >>  6) & 0x3F) * d1) - (((v1->color >>  6) & 0x3F) * d0)) / (d1 - d0);
    uint8_t b = ((((v0->color >> 12) & 0x3F) * d1) - (((v1->color >> 12) & 0x3F) * d0)) / (d1 - d0);
    vertex.color = (b << 12) | (g << 6) | r;

    return vertex;
}

bool Gpu3D::clipPolygon(Vertex *unclipped, Vertex *clipped, int side)
{
    bool clip = false;

    int vertexCount = polygonsIn[polygonCountIn].size;
    polygonsIn[polygonCountIn].size = 0;

    // Clip a polygon using the Sutherland-Hodgman algorithm
    for (int i = 0; i < vertexCount; i++)
    {
        // Get the unclipped vertices 
        Vertex *current = &unclipped[i];
        Vertex *previous = &unclipped[(i - 1 + vertexCount) % vertexCount];

        // Choose which coordinates to check based on the current side being clipped against
        int64_t currentVal, previousVal;
        switch (side)
        {
            case 0:  currentVal =  current->x; previousVal =  previous->x; break;
            case 1:  currentVal = -current->x; previousVal = -previous->x; break;
            case 2:  currentVal =  current->y; previousVal =  previous->y; break;
            case 3:  currentVal = -current->y; previousVal = -previous->y; break;
            case 4:  currentVal =  current->z; previousVal =  previous->z; break;
            default: currentVal = -current->z; previousVal = -previous->z; break;
        }

        // Add the clipped vertices
        if (currentVal >= -current->w) // Current vertex in bounds
        {
            if (previousVal < -previous->w) // Previous vertex not in bounds
            {
                clipped[polygonsIn[polygonCountIn].size] = intersection(current, previous, currentVal, previousVal);
                polygonsIn[polygonCountIn].size++;
                clip = true;
            }

            clipped[polygonsIn[polygonCountIn].size] = *current;
            polygonsIn[polygonCountIn].size++;
        }
        else if (previousVal >= -previous->w) // Previous vertex in bounds
        {
            clipped[polygonsIn[polygonCountIn].size] = intersection(current, previous, currentVal, previousVal);
            polygonsIn[polygonCountIn].size++;
            clip = true;
        }
    }

    return clip;
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
            if (projPointer < 1)
            {
                // Push to the single projection stack slot and increment the pointer
                projStack = projection;
                projPointer++;
            }
            else
            {
                // Indicate a matrix stack overflow error
                gxStat |= BIT(15);
            }
            break;

        case 1: case 2: // Coordinate and directional stacks
            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (coordPointer >= 30) gxStat |= BIT(15);

            // Push to the current coordinate and directional stack slots and increment the pointer
            if (coordPointer < 31)
            {
                coordStack[coordPointer] = coordinate;
                direcStack[coordPointer] = directional;
                coordPointer++;
            }
            break;

        case 3: // Texture stack
            // Push to the single texture stack slot
            // The texture stack doesn't have a pointer
            texStack = texture;
            break;
    }
}

void Gpu3D::mtxPopCmd(uint32_t param)
{
    // Pop a matrix from a stack
    switch (matrixMode)
    {
        case 0: // Projection stack
            if (projPointer > 0)
            {
                // Pop from the single projection stack slot and decrement the pointer
                // The parameter is ignored in projection mode
                projPointer--;
                projection = projStack;
                clipNeedsUpdate = true;
            }
            else
            {
                // Indicate a matrix stack underflow error
                gxStat |= BIT(15);
            }
            break;

        case 1: case 2: // Coordinate and directional stacks
        {
            // Calculate the stack address to pop from
            int address = coordPointer - (int8_t)(((param & BIT(5)) ? 0xC0 : 0) | (param & 0x0000003F));

            // Indicate a matrix stack underflow or overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (address < 0 || address >= 30) gxStat |= BIT(15);

            // Pop from the current coordinate and directional stack slots and update the pointer
            if (address >= 0 && address < 31)
            {
                coordinate = coordStack[address];
                directional = direcStack[address];
                coordPointer = address;
                clipNeedsUpdate = true;
            }
            break;
        }

        case 3: // Texture stack
            // Pop from the single texture stack slot
            // The texture stack doesn't have a pointer, and the parameter is ignored
            texture = texStack;
            break;
    }
}

void Gpu3D::mtxStoreCmd(uint32_t param)
{
    // Store a matrix to the stack
    switch (matrixMode)
    {
        case 0: // Projection stack
            // Store to the single projection stack slot
            // The parameter is ignored in projection mode
            projStack = projection;
            break;

        case 1: case 2: // Coordinate and directional stacks
        {
            // Get the stack address to store to
            int address = param & 0x0000001F;

            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (address == 31) gxStat |= BIT(15);

            // Store to the current coordinate and directional stack slots
            coordStack[address] = coordinate;
            direcStack[address] = directional;
            break;
       }

        case 3: // Texture stack
            // Store to the single texture stack slot
            // The parameter is ignored in texture mode
            texStack = texture;
            break;
    }
}

void Gpu3D::mtxRestoreCmd(uint32_t param)
{
    // Restore a matrix from the stack
    switch (matrixMode)
    {
        case 0: // Projection stack
            // Restore from the single projection stack slot
            // The parameter is ignored in projection mode
            projection = projStack;
            clipNeedsUpdate = true;
            break;

        case 1: case 2: // Coordinate and directional stacks
        {
            // Get the stack address to store to
            int address = param & 0x0000001F;

            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (address == 31) gxStat |= BIT(15);

            // Restore from the current coordinate and directional stack slots
            coordinate = coordStack[address];
            directional = direcStack[address];
            clipNeedsUpdate = true;
            break;
       }

        case 3: // Texture stack
            // Restore from the single texture stack slot
            // The parameter is ignored in texture mode
            texture = texStack;
            break;
    }
}

void Gpu3D::mtxIdentityCmd()
{
    // Set a matrix to the identity matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
            projection = Matrix();
            clipNeedsUpdate = true;
            break;

        case 1: // Coordinate stack
            coordinate = Matrix();
            clipNeedsUpdate = true;
            break;

        case 2: // Coordinate and directional stacks
            coordinate = Matrix();
            directional = Matrix();
            clipNeedsUpdate = true;
            break;

        case 3: // Texture stack
            texture = Matrix();
            break;
    }
}

void Gpu3D::mtxLoad44Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    temp.data[paramCount] = (int32_t)param;

    // Set a 4x4 matrix
    if (paramCount == 15)
    {
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = temp;
                clipNeedsUpdate = true;
                break;

            case 1: // Coordinate stack
                coordinate = temp;
                clipNeedsUpdate = true;
                break;

            case 2: // Coordinate and directional stack
                coordinate = temp;
                directional = temp;
                clipNeedsUpdate = true;
                break;

            case 3: // Texture stack
                texture = temp;
                break;
        }
    }
}

void Gpu3D::mtxLoad43Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[(paramCount / 3) * 4 + paramCount % 3] = (int32_t)param;

    // Set a 4x3 matrix
    if (paramCount == 11)
    {
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = temp;
                clipNeedsUpdate = true;
                break;

            case 1: // Coordinate stack
                coordinate = temp;
                clipNeedsUpdate = true;
                break;

            case 2: // Coordinate and directional stack
                coordinate = temp;
                directional = temp;
                clipNeedsUpdate = true;
                break;

            case 3: // Texture stack
                texture = temp;
                break;
        }
    }
}

void Gpu3D::mtxMult44Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    temp.data[paramCount] = (int32_t)param;

    // Multiply a matrix by a 4x4 matrix
    if (paramCount == 15)
    {
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                clipNeedsUpdate = true;
                break;

            case 1: // Coordinate stack
                coordinate = multiply(&temp, &coordinate);
                clipNeedsUpdate = true;
                break;

            case 2: // Coordinate and directional stacks
                coordinate = multiply(&temp, &coordinate);
                directional = multiply(&temp, &directional);
                clipNeedsUpdate = true;
                break;

            case 3: // Texture stack
                texture = multiply(&temp, &texture);
                break;
        }
    }
}

void Gpu3D::mtxMult43Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[(paramCount / 3) * 4 + paramCount % 3] = (int32_t)param;

    // Multiply a matrix by a 4x3 matrix
    if (paramCount == 11)
    {
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                clipNeedsUpdate = true;
                break;

            case 1: // Coordinate stack
                coordinate = multiply(&temp, &coordinate);
                clipNeedsUpdate = true;
                break;

            case 2: // Coordinate and directional stacks
                coordinate = multiply(&temp, &coordinate);
                directional = multiply(&temp, &directional);
                clipNeedsUpdate = true;
                break;

            case 3: // Texture stack
                texture = multiply(&temp, &texture);
                break;
        }
    }
}

void Gpu3D::mtxMult33Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[(paramCount / 3) * 4 + paramCount % 3] = (int32_t)param;

    // Multiply a matrix by a 3x3 matrix
    if (paramCount == 8)
    {
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                clipNeedsUpdate = true;
                break;

            case 1: // Coordinate stack
                coordinate = multiply(&temp, &coordinate);
                clipNeedsUpdate = true;
                break;

            case 2: // Coordinate and directional stacks
                coordinate = multiply(&temp, &coordinate);
                directional = multiply(&temp, &directional);
                clipNeedsUpdate = true;
                break;

            case 3: // Texture stack
                texture = multiply(&temp, &texture);
                break;
        }
    }
}

void Gpu3D::mtxScaleCmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[paramCount * 5] = (int32_t)param;

    // Multiply a matrix by a scale matrix
    if (paramCount == 2)
    {
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                clipNeedsUpdate = true;
                break;

            case 1: case 2: // Coordinate stack
                coordinate = multiply(&temp, &coordinate);
                clipNeedsUpdate = true;
                break;

            case 3: // Texture stack
                texture = multiply(&temp, &texture);
                break;
        }
    }
}

void Gpu3D::mtxTransCmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[12 + paramCount] = (int32_t)param;

    // Multiply a matrix by a translation matrix
    if (paramCount == 2)
    {
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                clipNeedsUpdate = true;
                break;

            case 1: // Coordinate stack
                coordinate = multiply(&temp, &coordinate);
                clipNeedsUpdate = true;
                break;

            case 2: // Coordinate and directional stacks
                coordinate = multiply(&temp, &coordinate);
                directional = multiply(&temp, &directional);
                clipNeedsUpdate = true;
                break;

            case 3: // Texture stack
                texture = multiply(&temp, &texture);
                break;
        }
    }
}

void Gpu3D::colorCmd(uint32_t param)
{
    // Set the vertex color
    savedColor = param;
}

void Gpu3D::texCoordCmd(uint32_t param)
{
    // Set the vertex texture coordinates
    if (((savedTexImageParam & 0xC0000000) >> 30) == 1) // TexCoord transformation
    {
        // Create a vertex with the texture coordinates
        Vertex vertex;
        vertex.x = ((int16_t)(param & 0x0000FFFF)) << 8;
        vertex.y = ((int16_t)((param & 0xFFFF0000) >> 16)) << 8;
        vertex.z = 1 << 8;
        vertex.w = 1 << 8;

        // Multiply the vertex with the texture matrix
        vertex = multiply(&vertex, &texture);

        // Save the transformed coordinates
        savedTexCoord = (((vertex.y >> 8) & 0xFFFF) << 16) | ((vertex.x >> 8) & 0xFFFF);
    }
    else
    {
        // Save the untransformed coordinates
        savedTexCoord = param;
    }
}

void Gpu3D::vtx16Cmd(uint32_t param)
{
    if (vertexCountIn >= 6144) return;

    if (paramCount == 0)
    {
        // Set the X and Y coordinates
        verticesIn[vertexCountIn].x = (int16_t)(param & 0x0000FFFF);
        verticesIn[vertexCountIn].y = (int16_t)((param & 0xFFFF0000) >> 16);
    }
    else
    {
        // Set the Z and W coordinates
        verticesIn[vertexCountIn].z = (int16_t)(param & 0x0000FFFF);
        verticesIn[vertexCountIn].w = 1 << 12;

        addVertex();
    }
}

void Gpu3D::vtx10Cmd(uint32_t param)
{
    if (vertexCountIn >= 6144) return;

    // Set the X, Y, Z and W coordinates
    verticesIn[vertexCountIn].x = (int16_t)((param & 0x000003FF) << 6);
    verticesIn[vertexCountIn].y = (int16_t)((param & 0x000FFC00) >> 4);
    verticesIn[vertexCountIn].z = (int16_t)((param & 0x3FF00000) >> 14);
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxXYCmd(uint32_t param)
{
    if (vertexCountIn >= 6144) return;

    // Set the X, Y, and W coordinates and get the Z coordinate from the previous vertex
    verticesIn[vertexCountIn].x = (int16_t)(param & 0x0000FFFF);
    verticesIn[vertexCountIn].y = (int16_t)((param & 0xFFFF0000) >> 16);
    verticesIn[vertexCountIn].z = last.z;
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxXZCmd(uint32_t param)
{
    if (vertexCountIn >= 6144) return;

    // Set the X, Z, and W coordinates and get the Y coordinate from the previous vertex
    verticesIn[vertexCountIn].x = (int16_t)(param & 0x0000FFFF);
    verticesIn[vertexCountIn].y = last.y;
    verticesIn[vertexCountIn].z = (int16_t)((param & 0xFFFF0000) >> 16);
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxYZCmd(uint32_t param)
{
    if (vertexCountIn >= 6144) return;

    // Set the Y, Z, and W coordinates and get the X coordinate from the previous vertex
    verticesIn[vertexCountIn].x = last.x;
    verticesIn[vertexCountIn].y = (int16_t)(param & 0x0000FFFF);
    verticesIn[vertexCountIn].z = (int16_t)((param & 0xFFFF0000) >> 16);
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxDiffCmd(uint32_t param)
{
    if (vertexCountIn >= 6144) return;

    // Get the X, Y and Z offsets
    Vertex vertex;
    vertex.x = ((int16_t)((param & 0x000003FF) << 6)  / 8) >> 3;
    vertex.y = ((int16_t)((param & 0x000FFC00) >> 4)  / 8) >> 3;
    vertex.z = ((int16_t)((param & 0x3FF00000) >> 14) / 8) >> 3;

    // Add the offsets to the previous vertex to create a new vertex
    verticesIn[vertexCountIn].x = last.x + vertex.x;
    verticesIn[vertexCountIn].y = last.y + vertex.y;
    verticesIn[vertexCountIn].z = last.z + vertex.z;
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::texImageParamCmd(uint32_t param)
{
    // Set the texture image parameters
    savedTexImageParam = param;
}

void Gpu3D::plttBaseCmd(uint32_t param)
{
    // Set the texture palette base
    savedPlttBase = param;
}

void Gpu3D::beginVtxsCmd(uint32_t param)
{
    // Clipping a polygon strip starts a new strip with the last 2 vertices of the old one
    // Discard these vertices if they're unused
    if (size == 2 && vertexCountIn >= 2)
        vertexCountIn -= 2;

    // Begin a new vertex list
    savedBeginVtxs = param & 0x00000003;
    size = 0;
}

void Gpu3D::swapBuffersCmd(uint32_t param)
{
    // Halt the geometry engine
    // The buffers will be swapped and the engine unhalted on next V-blank
    halted = true;
}

void Gpu3D::addEntry(Entry entry)
{
    if (fifo.empty() && pipe.size() < 4)
    {
        // Move data directly into the PIPE if the FIFO is empty and the PIPE isn't full
        pipe.push(entry);

        // Update the FIFO status
        gxStat |= BIT(27); // Commands executing
    }
    else
    {
        // If the FIFO is full, free space by running cycles
        // On real hardware, a GXFIFO overflow would halt the CPU until space is free
        while (fifo.size() >= 256)
            runCycle();

        // Move data into the FIFO
        fifo.push(entry);

        // Update the FIFO status
        gxStat = (gxStat & ~0x01FF0000) | (fifo.size() << 16); // Count
        if (fifo.size() >= 128) gxStat &= ~BIT(25); // Half or more full
        gxStat &= ~BIT(26); // Not empty
    }
}

void Gpu3D::writeGxFifo(unsigned int byte, uint8_t value)
{
    // Write to the GXFIFO register
    gxFifo = (gxFifo & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        if (gxFifoCmds == 0)
        {
            // Start a transfer
            gxFifoCmds = gxFifo;
        }
        else
        {
            // Add a command parameter
            Entry entry = { (uint8_t)gxFifoCmds, gxFifo };
            addEntry(entry);
            gxFifoCount++;

            // Move to the next command once all parameters have been sent
            if (gxFifoCount == paramCounts[gxFifoCmds & 0xFF])
            {
                gxFifoCmds >>= 8;
                gxFifoCount = 0;
            }
        }

        // Add entries for commands with no parameters
        while (gxFifoCmds != 0 && paramCounts[gxFifoCmds & 0xFF] == 0)
        {
            Entry entry = { (uint8_t)gxFifoCmds, 0 };
            addEntry(entry);
            gxFifoCmds >>= 8;
        }
    }
}

void Gpu3D::writeMtxMode(unsigned int byte, uint8_t value)
{
    // Write to the MTX_MODE register
    mtxMode = (mtxMode & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x10, mtxMode };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxPush(unsigned int byte, uint8_t value)
{
    // Write to the MTX_PUSH register
    mtxPush = (mtxPush & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x11, mtxPush };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxPop(unsigned int byte, uint8_t value)
{
    // Write to the MTX_POP register
    mtxPop = (mtxPop & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x12, mtxPop };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxStore(unsigned int byte, uint8_t value)
{
    // Write to the MTX_STORE register
    mtxStore = (mtxStore & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x13, mtxStore };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxRestore(unsigned int byte, uint8_t value)
{
    // Write to the MTX_RESTORE register
    mtxRestore = (mtxRestore & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x14, mtxRestore };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxIdentity(unsigned int byte, uint8_t value)
{
    // Write to the MTX_IDENTITY register
    mtxIdentity = (mtxIdentity & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x15, mtxIdentity };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxLoad44(unsigned int byte, uint8_t value)
{
    // Write to the MTX_LOAD_4x4 register
    mtxLoad44 = (mtxLoad44 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x16, mtxLoad44 };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxLoad43(unsigned int byte, uint8_t value)
{
    // Write to the MTX_LOAD_4x3 register
    mtxLoad43 = (mtxLoad43 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x17, mtxLoad43 };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxMult44(unsigned int byte, uint8_t value)
{
    // Write to the MTX_MULT_4x4 register
    mtxMult44 = (mtxMult44 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x18, mtxMult44 };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxMult43(unsigned int byte, uint8_t value)
{
    // Write to the MTX_MULT_4x3 register
    mtxMult43 = (mtxMult43 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x19, mtxMult43 };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxMult33(unsigned int byte, uint8_t value)
{
    // Write to the MTX_MULT_3x3 register
    mtxMult33 = (mtxMult33 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x1A, mtxMult33 };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxScale(unsigned int byte, uint8_t value)
{
    // Write to the MTX_SCALE register
    mtxScale = (mtxScale & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x1B, mtxScale };
        addEntry(entry);
    }
}

void Gpu3D::writeMtxTrans(unsigned int byte, uint8_t value)
{
    // Write to the MTX_TRANS register
    mtxTrans = (mtxTrans & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x1C, mtxTrans };
        addEntry(entry);
    }
}

void Gpu3D::writeColor(unsigned int byte, uint8_t value)
{
    // Write to the COLOR register
    color = (color & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x20, color };
        addEntry(entry);
    }
}

void Gpu3D::writeNormal(unsigned int byte, uint8_t value)
{
    // Write to the NORMAL register
    normal = (normal & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x21, normal };
        addEntry(entry);
    }
}

void Gpu3D::writeTexCoord(unsigned int byte, uint8_t value)
{
    // Write to the TEXCOORD register
    texCoord = (texCoord & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x22, texCoord };
        addEntry(entry);
    }
}

void Gpu3D::writeVtx16(unsigned int byte, uint8_t value)
{
    // Write to the VTX_16 register
    vtx16 = (vtx16 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x23, vtx16 };
        addEntry(entry);
    }
}

void Gpu3D::writeVtx10(unsigned int byte, uint8_t value)
{
    // Write to the VTX_10 register
    vtx10 = (vtx10 & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x24, vtx10 };
        addEntry(entry);
    }
}

void Gpu3D::writeVtxXY(unsigned int byte, uint8_t value)
{
    // Write to the VTX_XY register
    vtxXY = (vtxXY & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x25, vtxXY };
        addEntry(entry);
    }
}

void Gpu3D::writeVtxXZ(unsigned int byte, uint8_t value)
{
    // Write to the VTX_XZ register
    vtxXZ = (vtxXZ & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x26, vtxXZ };
        addEntry(entry);
    }
}

void Gpu3D::writeVtxYZ(unsigned int byte, uint8_t value)
{
    // Write to the VTX_XZ register
    vtxYZ = (vtxYZ & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x27, vtxYZ };
        addEntry(entry);
    }
}

void Gpu3D::writeVtxDiff(unsigned int byte, uint8_t value)
{
    // Write to the VTX_DIFF register
    vtxDiff = (vtxDiff & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x28, vtxDiff };
        addEntry(entry);
    }
}

void Gpu3D::writePolygonAttr(unsigned int byte, uint8_t value)
{
    // Write to the POLYGON_ATTR register
    polygonAttr = (polygonAttr & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x29, polygonAttr };
        addEntry(entry);
    }
}

void Gpu3D::writeTexImageParam(unsigned int byte, uint8_t value)
{
    // Write to the TEXIMAGE_PARAM register
    texImageParam = (texImageParam & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x2A, texImageParam };
        addEntry(entry);
    }
}

void Gpu3D::writePlttBase(unsigned int byte, uint8_t value)
{
    // Write to the PLTT_BASE register
    plttBase = (plttBase & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x2B, plttBase };
        addEntry(entry);
    }
}

void Gpu3D::writeDifAmb(unsigned int byte, uint8_t value)
{
    // Write to the DIF_AMB register
    difAmb = (difAmb & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x30, difAmb };
        addEntry(entry);
    }
}

void Gpu3D::writeSpeEmi(unsigned int byte, uint8_t value)
{
    // Write to the SPE_EMI register
    speEmi = (speEmi & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x31, speEmi };
        addEntry(entry);
    }
}

void Gpu3D::writeLightVector(unsigned int byte, uint8_t value)
{
    // Write to the LIGHT_VECTOR register
    lightVector = (lightVector & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x32, lightVector };
        addEntry(entry);
    }
}

void Gpu3D::writeLightColor(unsigned int byte, uint8_t value)
{
    // Write to the LIGHT_COLOR register
    lightColor = (lightColor & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x33, lightColor };
        addEntry(entry);
    }
}

void Gpu3D::writeShininess(unsigned int byte, uint8_t value)
{
    // Write to the SHININESS register
    shininess = (shininess & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x34, shininess };
        addEntry(entry);
    }
}

void Gpu3D::writeBeginVtxs(unsigned int byte, uint8_t value)
{
    // Write to the BEGIN_VTXS register
    beginVtxs = (beginVtxs & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x40, beginVtxs };
        addEntry(entry);
    }
}

void Gpu3D::writeEndVtxs(unsigned int byte, uint8_t value)
{
    // Write to the END_VTXS register
    endVtxs = (endVtxs & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x41, endVtxs };
        addEntry(entry);
    }
}

void Gpu3D::writeSwapBuffers(unsigned int byte, uint8_t value)
{
    // Write to the SWAP_BUFFERS register
    _swapBuffers = (_swapBuffers & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x50, _swapBuffers };
        addEntry(entry);
    }
}

void Gpu3D::writeViewport(unsigned int byte, uint8_t value)
{
    // Write to the VIEWPORT register
    viewport = (viewport & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x60, viewport };
        addEntry(entry);
    }
}

void Gpu3D::writeBoxTest(unsigned int byte, uint8_t value)
{
    // Write to the BOX_TEST register
    boxTest = (boxTest & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x70, boxTest };
        addEntry(entry);
    }
}

void Gpu3D::writePosTest(unsigned int byte, uint8_t value)
{
    // Write to the POS_TEST register
    posTest = (posTest & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x71, posTest };
        addEntry(entry);
    }
}

void Gpu3D::writeVecTest(unsigned int byte, uint8_t value)
{
    // Write to the VEC_TEST register
    vecTest = (vecTest & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x72, vecTest };
        addEntry(entry);
    }
}

void Gpu3D::writeGxStat(unsigned int byte, uint8_t value)
{
    // Clear the error bit and reset the projection stack pointer
    if (byte == 1 && (value & BIT(7)))
        gxStat &= ~0x0000A000;

    // Write to the GXSTAT register
    uint32_t mask = 0xC0000000 & (0xFF << (byte * 8));
    gxStat = (gxStat & ~mask) | ((value << (byte * 8)) & mask);
}

uint8_t Gpu3D::readClipMtxResult(unsigned int index, unsigned int byte)
{
    // Update the clip matrix if necessary
    if (clipNeedsUpdate)
    {
        clip = multiply(&coordinate, &projection);
        clipNeedsUpdate = false;
    }

    // Read from one of the CLIPMTX_RESULT registers
    return clip.data[index] >> (byte * 8);
}
