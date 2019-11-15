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
        case 0x10: mtxModeCmd(entry.param);     break; // MTX_MODE
        case 0x11: mtxPushCmd();                break; // MTX_PUSH
        case 0x12: mtxPopCmd(entry.param);      break; // MTX_POP
        case 0x13: mtxStoreCmd(entry.param);    break; // MTX_STORE
        case 0x14: mtxRestoreCmd(entry.param);  break; // MTX_RESTORE
        case 0x15: mtxIdentityCmd();            break; // MTX_IDENTITY
        case 0x16: mtxLoad44Cmd(entry.param);   break; // MTX_LOAD_4x4
        case 0x17: mtxLoad43Cmd(entry.param);   break; // MTX_LOAD_4x3
        case 0x18: mtxMult44Cmd(entry.param);   break; // MTX_MULT_4x4
        case 0x19: mtxMult43Cmd(entry.param);   break; // MTX_MULT_4x3
        case 0x1A: mtxMult33Cmd(entry.param);   break; // MTX_MULT_3x3
        case 0x1B: mtxScaleCmd(entry.param);    break; // MTX_SCALE
        case 0x1C: mtxTransCmd(entry.param);    break; // MTX_TRANS
        case 0x20: colorCmd(entry.param);       break; // COLOR
        case 0x23: vtx16Cmd(entry.param);       break; // VTX_16
        case 0x24: vtx10Cmd(entry.param);       break; // VTX_10
        case 0x25: vtxXYCmd(entry.param);       break; // VTX_XY
        case 0x26: vtxXZCmd(entry.param);       break; // VTX_XZ
        case 0x27: vtxYZCmd(entry.param);       break; // VTX_YZ
        case 0x28: vtxDiffCmd(entry.param);     break; // VTX_DIFF
        case 0x40: beginVtxsCmd(entry.param);   break; // BEGIN_VTXS
        case 0x41:                              break; // END_VTXS
        case 0x50: swapBuffersCmd(entry.param); break; // SWAP_BUFFERS

        default:
            printf("Unknown GXFIFO command: 0x%X\n", entry.command);
            break;
    }

    // Count how many parameters have been used
    paramCount++;
    if (paramCount >= paramCounts[entry.command])
        paramCount = 0;

    // Move 2 FIFO entries into the PIPE if it runs half empty
    if (pipe.size() < 3)
    {
        for (int i = 0; i < ((fifo.size() > 2) ? 2 : fifo.size()); i++)
        {
            entry = fifo.front();
            fifo.pop();
            pipe.push(entry);
        }
    }

    // Update the FIFO status
    gxStat = (gxStat & ~0x01FF0000) | (fifo.size() << 16); // Count
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

Matrix Gpu3D::multiply(Matrix *mtx1, Matrix *mtx2)
{
    Matrix matrix = {{}};

    // Multiply 2 matrices
    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            for (int i = 0; i < 4; i++)
                matrix.data[y * 4 + x] += ((int64_t)mtx1->data[y * 4 + i] * mtx2->data[i * 4 + x]) >> 12;
        }
    }

    return matrix;
}

Vertex Gpu3D::multiply(Vertex *vtx, Matrix *mtx)
{
    Vertex vertex = *vtx;

    // Multiply a vertex with a matrix
    vertex.x = ((int64_t)vtx->x * mtx->data[0] + (int64_t)vtx->y * mtx->data[4] + (int64_t)vtx->z * mtx->data[8]  + (int64_t)vtx->w * mtx->data[12]) >> 12;
    vertex.y = ((int64_t)vtx->x * mtx->data[1] + (int64_t)vtx->y * mtx->data[5] + (int64_t)vtx->z * mtx->data[9]  + (int64_t)vtx->w * mtx->data[13]) >> 12;
    vertex.z = ((int64_t)vtx->x * mtx->data[2] + (int64_t)vtx->y * mtx->data[6] + (int64_t)vtx->z * mtx->data[10] + (int64_t)vtx->w * mtx->data[14]) >> 12;
    vertex.w = ((int64_t)vtx->x * mtx->data[3] + (int64_t)vtx->y * mtx->data[7] + (int64_t)vtx->z * mtx->data[11] + (int64_t)vtx->w * mtx->data[15]) >> 12;

    return vertex;
}

void Gpu3D::addVertex()
{
    last = verticesIn[vertexCountIn];

    // Transform the vertex
    verticesIn[vertexCountIn] = multiply(&verticesIn[vertexCountIn], &coordStack[(gxStat & 0x00001F00) >> 8]);
    verticesIn[vertexCountIn] = multiply(&verticesIn[vertexCountIn], &projection);

    // Move to the next polygon if one has been completed
    if (size != 0 && polygonCountIn > 0 && polygonCountIn < 2048)
    {
        switch (polygonsIn[polygonCountIn - 1].type)
        {
            case 0: // Separate triangles
                if (size % 3 == 0)
                {
                    polygonsIn[polygonCountIn].type = 0;
                    polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn];
                    polygonCountIn++;
                }
                break;

            case 1: // Separate quads
                if (size % 4 == 0)
                {
                    polygonsIn[polygonCountIn].type = 1;
                    polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn];
                    polygonCountIn++;
                }
                break;

            case 2: // Triangle strips
                if (size >= 3)
                {
                    polygonsIn[polygonCountIn].type = 2;
                    polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn - 2];
                    polygonCountIn++;
                }
                break;

            case 3: // Quad strips
                if (size >= 4 && size % 2 == 0)
                {
                    polygonsIn[polygonCountIn].type = 3;
                    polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn - 2];
                    polygonCountIn++;
                }
                break;
        }
    }

    // Move to the next vertex
    if (vertexCountIn < 6143)
    {
        vertexCountIn++;
        verticesIn[vertexCountIn].color = verticesIn[vertexCountIn - 1].color;
        size++;
    }
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
            // Increment the stack pointer or indicate an overflow error
            // There's only one projection matrix, so no copying is needed
            gxStat |= BIT((gxStat & BIT(13)) ? 15 : 13);
            break;

        case 1: case 2: // Coordinate and directional stacks
        {
            int pointer = (gxStat & 0x00001F00) >> 8;

            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (pointer >= 30) gxStat |= BIT(15);

            if (pointer < 31)
            {
                // Increment the pointer and copy the old current matrix to the new current matrix
                pointer++;
                coordStack[pointer] = coordStack[pointer - 1];
                direcStack[pointer] = direcStack[pointer - 1];
                gxStat = (gxStat & ~0x00001F00) | ((pointer & 0x1F) << 8);
            }

            break;
        }

        case 3: // Texture stack
            printf("Unhandled GXFIFO matrix command in texture mode\n");
            break;
    }
}

void Gpu3D::mtxPopCmd(uint32_t param)
{
    // Pop a matrix from a stack
    switch (matrixMode)
    {
        case 0: // Projection stack
            // Decrement the stack pointer or indicate an underflow error
            if (gxStat & BIT(13)) gxStat &= ~BIT(13); else gxStat |= BIT(15);
            break;

        case 1: case 2: // Coordinate and directional stacks
        {
            int pointer = (gxStat & 0x00001F00) >> 8;
            int8_t offset = ((param & BIT(5)) ? 0xC0 : 0) | (param & 0x0000003F);

            if (pointer >= offset)
            {
                // Decrement the stack pointer
                pointer -= offset;
                gxStat = (gxStat & ~0x00001F00) | ((pointer & 0x1F) << 8);
            }
            else
            {
                // Indicate a matrix stack underflow error
                gxStat |= BIT(15);
            }

            break;
       }

        case 3: // Texture stack
            printf("Unhandled GXFIFO matrix command in texture mode\n");
            break;
    }
}

void Gpu3D::mtxStoreCmd(uint32_t param)
{
    // Store a matrix to the stack
    switch (matrixMode)
    {
        case 1: case 2: // Coordinate and directional stacks
        {
            int pointer = (gxStat & 0x00001F00) >> 8;
            int offset = param & 0x0000001F;

            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (offset == 31) gxStat |= BIT(15);

            coordStack[offset] = coordStack[pointer];
            direcStack[offset] = direcStack[pointer];

            break;
       }

        case 3: // Texture stack
            printf("Unhandled GXFIFO matrix command in texture mode\n");
            break;
    }
}

void Gpu3D::mtxRestoreCmd(uint32_t param)
{
    // Restore a matrix from the stack
    switch (matrixMode)
    {
        case 1: case 2: // Coordinate and directional stacks
        {
            int pointer = (gxStat & 0x00001F00) >> 8;
            int offset = param & 0x0000001F;

            // Indicate a matrix stack overflow error
            // Even though the 31st slot exists, it still causes an overflow error
            if (pointer == 31) gxStat |= BIT(15);

            coordStack[pointer] = coordStack[offset];
            direcStack[pointer] = direcStack[offset];

            break;
       }

        case 3: // Texture stack
            printf("Unhandled GXFIFO matrix command in texture mode\n");
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
            break;

        case 1: // Coordinate stack
            coordStack[(gxStat & 0x00001F00) >> 8] = Matrix();
            break;

        case 2: // Coordinate and directional stacks
        {
            int pointer = (gxStat & 0x00001F00) >> 8;
            coordStack[pointer] = Matrix();
            direcStack[pointer] = Matrix();
            break;
        }

        case 3: // Texture stack
            printf("Unhandled GXFIFO matrix command in texture mode\n");
            break;
    }
}

void Gpu3D::mtxLoad44Cmd(uint32_t param)
{
    // Set a 4x4 matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
            projection.data[paramCount] = param;
            break;

        case 1: // Coordinate stack
            coordStack[(gxStat & 0x00001F00) >> 8].data[paramCount] = param;
            break;

        case 2: // Coordinate and directional stack
        {
            int pointer = (gxStat & 0x00001F00) >> 8;
            coordStack[pointer].data[paramCount] = param;
            direcStack[pointer].data[paramCount] = param;
            break;
        }

        case 3: // Texture stack
            printf("Unhandled GXFIFO matrix command in texture mode\n");
            break;
    }
}

void Gpu3D::mtxLoad43Cmd(uint32_t param)
{
    // Set a 4x3 matrix
    switch (matrixMode)
    {
        case 0: // Projection stack
            if (paramCount == 0) projection = Matrix();
            projection.data[(paramCount / 3) * 4 + paramCount % 3] = param;
            break;

        case 1: // Coordinate stack
        {
            int pointer = (gxStat & 0x00001F00) >> 8;
            if (paramCount == 0) coordStack[pointer] = Matrix();
            coordStack[pointer].data[(paramCount / 3) * 4 + paramCount % 3] = param;
            break;
        }

        case 2: // Coordinate and directional stack
        {
            int pointer = (gxStat & 0x00001F00) >> 8;
            if (paramCount == 0) coordStack[pointer] = Matrix();
            coordStack[pointer].data[(paramCount / 3) * 4 + paramCount % 3] = param;
            if (paramCount == 11) direcStack[pointer] = coordStack[pointer];
            break;
        }

        case 3: // Texture stack
            printf("Unhandled GXFIFO matrix command in texture mode\n");
            break;
    }
}

void Gpu3D::mtxMult44Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    temp.data[paramCount] = param;

    if (paramCount == 15)
    {
        // Multiply a matrix by a 4x4 matrix
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                break;

            case 1: // Coordinate stack
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                break;
            }

            case 2: // Coordinate and directional stacks
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                direcStack[pointer] = multiply(&temp, &direcStack[pointer]);
                break;
            }

            case 3: // Texture stack
                printf("Unhandled GXFIFO matrix command in texture mode\n");
                break;
        }
    }
}

void Gpu3D::mtxMult43Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[(paramCount / 3) * 4 + paramCount % 3] = param;

    if (paramCount == 11)
    {
        // Multiply a matrix by a 4x3 matrix
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                break;

            case 1: // Coordinate stack
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                break;
            }

            case 2: // Coordinate and directional stacks
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                direcStack[pointer] = multiply(&temp, &direcStack[pointer]);
                break;
            }

            case 3: // Texture stack
                printf("Unhandled GXFIFO matrix command in texture mode\n");
                break;
        }
    }
}

void Gpu3D::mtxMult33Cmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[(paramCount / 3) * 4 + paramCount % 3] = param;

    if (paramCount == 8)
    {
        // Multiply a matrix by a 3x3 matrix
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                break;

            case 1: // Coordinate stack
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                break;
            }

            case 2: // Coordinate and directional stacks
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                direcStack[pointer] = multiply(&temp, &direcStack[pointer]);
                break;
            }

            case 3: // Texture stack
                printf("Unhandled GXFIFO matrix command in texture mode\n");
                break;
        }
    }
}

void Gpu3D::mtxScaleCmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[paramCount * 5] = param;

    if (paramCount == 2)
    {
        // Multiply a matrix by a scale matrix
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                break;

            case 1: case 2: // Coordinate stack
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                break;
            }

            case 3: // Texture stack
                printf("Unhandled GXFIFO matrix command in texture mode\n");
                break;
        }
    }
}

void Gpu3D::mtxTransCmd(uint32_t param)
{
    // Store the paramaters to the temporary matrix
    if (paramCount == 0) temp = Matrix();
    temp.data[12 + paramCount] = param;

    if (paramCount == 2)
    {
        // Multiply a matrix by a translation matrix
        switch (matrixMode)
        {
            case 0: // Projection stack
                projection = multiply(&temp, &projection);
                break;

            case 1: // Coordinate stack
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                break;
            }

            case 2: // Coordinate and directional stacks
            {
                int pointer = (gxStat & 0x00001F00) >> 8;
                coordStack[pointer] = multiply(&temp, &coordStack[pointer]);
                direcStack[pointer] = multiply(&temp, &direcStack[pointer]);
                break;
            }

            case 3: // Texture stack
                printf("Unhandled GXFIFO matrix command in texture mode\n");
                break;
        }
    }
}

void Gpu3D::colorCmd(uint32_t param)
{
    // Set the vertex color
    verticesIn[vertexCountIn].color = param & 0x00007FFF;
}

void Gpu3D::vtx16Cmd(uint32_t param)
{
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
    // Set the X, Y, Z and W coordinates
    verticesIn[vertexCountIn].x = (int16_t)((param & 0x000003FF) << 6);
    verticesIn[vertexCountIn].y = (int16_t)((param & 0x000FFC00) >> 4);
    verticesIn[vertexCountIn].z = (int16_t)((param & 0x3FF00000) >> 14);
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxXYCmd(uint32_t param)
{
    // Set the X, Y, and W coordinates and get the Z coordinate from the previous vertex
    verticesIn[vertexCountIn].x = (int16_t)(param & 0x0000FFFF);
    verticesIn[vertexCountIn].y = (int16_t)((param & 0xFFFF0000) >> 16);
    verticesIn[vertexCountIn].z = last.z;
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxXZCmd(uint32_t param)
{
    // Set the X, Z, and W coordinates and get the Y coordinate from the previous vertex
    verticesIn[vertexCountIn].x = (int16_t)(param & 0x0000FFFF);
    verticesIn[vertexCountIn].y = last.y;
    verticesIn[vertexCountIn].z = (int16_t)((param & 0xFFFF0000) >> 16);
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxYZCmd(uint32_t param)
{
    // Set the Y, Z, and W coordinates and get the X coordinate from the previous vertex
    verticesIn[vertexCountIn].x = last.x;
    verticesIn[vertexCountIn].y = (int16_t)(param & 0x0000FFFF);
    verticesIn[vertexCountIn].z = (int16_t)((param & 0xFFFF0000) >> 16);
    verticesIn[vertexCountIn].w = 1 << 12;

    addVertex();
}

void Gpu3D::vtxDiffCmd(uint32_t param)
{
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

void Gpu3D::beginVtxsCmd(uint32_t param)
{
    // Start a new polygon
    if (polygonCountIn < 2048)
    {
        polygonsIn[polygonCountIn].type = param & 0x00000003;
        polygonsIn[polygonCountIn].vertices = &verticesIn[vertexCountIn];
        polygonCountIn++;
        size = 0;
    }
}

void Gpu3D::swapBuffersCmd(uint32_t param)
{
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
    else if (fifo.size() < 256)
    {
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
    swapBuffers = (swapBuffers & ~(0xFF << (byte * 8))) | (value << (byte * 8));

    if (byte == 3)
    {
        // Add an entry to the FIFO
        Entry entry = { 0x50, swapBuffers };
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
