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
    // Pretend to execute a command
    Entry entry = pipe.front();
    pipe.pop();
    printf("Unhandled GXFIFO command: 0x%X\n", entry.command);

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
        Entry entry = { 0x40, difAmb };
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
