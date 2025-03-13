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

#include <cmath>

#include "div_sqrt.h"
#include "core.h"

void DivSqrt::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(&divCnt, sizeof(divCnt), 1, file);
    fwrite(&divNumer, sizeof(divNumer), 1, file);
    fwrite(&divDenom, sizeof(divDenom), 1, file);
    fwrite(&divResult, sizeof(divResult), 1, file);
    fwrite(&divRemResult, sizeof(divRemResult), 1, file);
    fwrite(&sqrtCnt, sizeof(sqrtCnt), 1, file);
    fwrite(&sqrtResult, sizeof(sqrtResult), 1, file);
    fwrite(&sqrtParam, sizeof(sqrtParam), 1, file);
}

void DivSqrt::loadState(FILE *file)
{
    // Read state data from the file
    fread(&divCnt, sizeof(divCnt), 1, file);
    fread(&divNumer, sizeof(divNumer), 1, file);
    fread(&divDenom, sizeof(divDenom), 1, file);
    fread(&divResult, sizeof(divResult), 1, file);
    fread(&divRemResult, sizeof(divRemResult), 1, file);
    fread(&sqrtCnt, sizeof(sqrtCnt), 1, file);
    fread(&sqrtResult, sizeof(sqrtResult), 1, file);
    fread(&sqrtParam, sizeof(sqrtParam), 1, file);
}

void DivSqrt::divide()
{
    // Set the division by zero bit based on the full 64-bit denominator, even in 32-bit mode
    divDenom ? (divCnt &= ~BIT(14)) : (divCnt |= BIT(14));

    // Calculate the division result and remainder
    switch (divCnt & 0x3) // Division mode
    {
        case 0: // 32-bit / 32-bit
        {
            if ((int32_t)divNumer == INT32_MIN && (int32_t)divDenom == -1) // Overflow
            {
                divResult = (int32_t)divNumer ^ (0xFFFFFFFFULL << 32);
                divRemResult = 0;
            }
            else if ((int32_t)divDenom != 0)
            {
                divResult = (int32_t)divNumer / (int32_t)divDenom;
                divRemResult = (int32_t)divNumer % (int32_t)divDenom;
            }
            else // Division by 0
            {
                divResult = (((int32_t)divNumer < 0) ? 1 : -1) ^ (0xFFFFFFFFULL << 32);
                divRemResult = (int32_t)divNumer;
            }
            break;
        }

        case 1: case 3: // 64-bit / 32-bit
        {
            if (divNumer == INT64_MIN && (int32_t)divDenom == -1) // Overflow
            {
                divResult = divNumer;
                divRemResult = 0;
            }
            else if ((int32_t)divDenom != 0)
            {
                divResult = divNumer / (int32_t)divDenom;
                divRemResult = divNumer % (int32_t)divDenom;
            }
            else // Division by 0
            {
                divResult = (divNumer < 0) ? 1 : -1;
                divRemResult = divNumer;
            }
            break;
        }

        case 2: // 64-bit / 64-bit
        {
            if (divNumer == INT64_MIN && divDenom == -1) // Overflow
            {
                divResult = divNumer;
                divRemResult = 0;
            }
            else if (divDenom != 0)
            {
                divResult = divNumer / divDenom;
                divRemResult = divNumer % divDenom;
            }
            else // Division by 0
            {
                divResult = (divNumer < 0) ? 1 : -1;
                divRemResult = divNumer;
            }
            break;
        }
    }
}

void DivSqrt::squareRoot()
{
    // Calculate the square root result
    switch (sqrtCnt & 0x1) // Square root mode
    {
        case 0: // 32-bit
            sqrtResult = sqrt((uint32_t)sqrtParam);
            break;

        case 1: // 64-bit
            sqrtResult = sqrtl(sqrtParam);
            break;
    }
}

void DivSqrt::writeDivCnt(uint16_t mask, uint16_t value)
{
    // Write to the DIVCNT register and update the division result
    mask &= 0x0003;
    divCnt = (divCnt & ~mask) | (value & mask);
    divide();
}

void DivSqrt::writeDivNumerL(uint32_t mask, uint32_t value)
{
    // Write to the DIVNUMER register and update the division result
    divNumer = (divNumer & ~((uint64_t)mask)) | (value & mask);
    divide();
}

void DivSqrt::writeDivNumerH(uint32_t mask, uint32_t value)
{
    // Write to the DIVNUMER register and update the division result
    divNumer = (divNumer & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);
    divide();
}

void DivSqrt::writeDivDenomL(uint32_t mask, uint32_t value)
{
    // Write to the DIVDENOM register and update the division result
    divDenom = (divDenom & ~((uint64_t)mask)) | (value & mask);
    divide();
}

void DivSqrt::writeDivDenomH(uint32_t mask, uint32_t value)
{
    // Write to the DIVDENOM register and update the division result
    divDenom = (divDenom & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);
    divide();
}

void DivSqrt::writeSqrtCnt(uint16_t mask, uint16_t value)
{
    // Write to the SQRTCNT register and update the square root result
    mask &= 0x0001;
    sqrtCnt = (sqrtCnt & ~mask) | (value & mask);
    squareRoot();
}

void DivSqrt::writeSqrtParamL(uint32_t mask, uint32_t value)
{
    // Write to the DIVDENOM register and update the square root result
    sqrtParam = (sqrtParam & ~((uint64_t)mask)) | (value & mask);
    squareRoot();
}

void DivSqrt::writeSqrtParamH(uint32_t mask, uint32_t value)
{
    // Write to the SQRTPARAM register and update the square root result
    sqrtParam = (sqrtParam & ~((uint64_t)mask << 32)) | ((uint64_t)(value & mask) << 32);
    squareRoot();
}
