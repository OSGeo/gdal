/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  RAT utility
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "rat.h"

/************************************************************************/
/*                             CreateRAT()                              */
/************************************************************************/

std::unique_ptr<GDALRasterAttributeTable>
HDF5CreateRAT(const std::shared_ptr<GDALMDArray> &poValues,
              bool bFirstColIsMinMax)
{
    auto poRAT = std::make_unique<GDALDefaultRasterAttributeTable>();
    const auto &poComponents = poValues->GetDataType().GetComponents();
    for (const auto &poComponent : poComponents)
    {
        GDALRATFieldType eType;
        if (poComponent->GetType().GetClass() == GEDTC_NUMERIC)
        {
            if (GDALDataTypeIsInteger(
                    poComponent->GetType().GetNumericDataType()))
                eType = GFT_Integer;
            else
                eType = GFT_Real;
        }
        else
        {
            eType = GFT_String;
        }
        poRAT->CreateColumn(poComponent->GetName().c_str(), eType,
                            bFirstColIsMinMax && poRAT->GetColumnCount() == 0
                                ? GFU_MinMax
                                : GFU_Generic);
    }

    const auto &oValuesDT = poValues->GetDataType();
    std::vector<GByte> abyRow(oValuesDT.GetSize());
    const int nRows = static_cast<int>(poValues->GetDimensions()[0]->GetSize());
    for (int iRow = 0; iRow < nRows; iRow++)
    {
        const GUInt64 arrayStartIdx = static_cast<GUInt64>(iRow);
        const size_t count = 1;
        const GInt64 arrayStep = 0;
        const GPtrDiff_t bufferStride = 0;
        poValues->Read(&arrayStartIdx, &count, &arrayStep, &bufferStride,
                       oValuesDT, &abyRow[0]);
        int iCol = 0;
        for (const auto &poComponent : poComponents)
        {
            const auto eRATType = poRAT->GetTypeOfCol(iCol);
            if (eRATType == GFT_Integer)
            {
                int nValue = 0;
                GDALCopyWords(&abyRow[poComponent->GetOffset()],
                              poComponent->GetType().GetNumericDataType(), 0,
                              &nValue, GDT_Int32, 0, 1);
                poRAT->SetValue(iRow, iCol, nValue);
            }
            else if (eRATType == GFT_Real)
            {
                double dfValue = 0;
                GDALCopyWords(&abyRow[poComponent->GetOffset()],
                              poComponent->GetType().GetNumericDataType(), 0,
                              &dfValue, GDT_Float64, 0, 1);
                poRAT->SetValue(iRow, iCol, dfValue);
            }
            else
            {
                char *pszStr = nullptr;
                GDALExtendedDataType::CopyValue(
                    &abyRow[poComponent->GetOffset()], poComponent->GetType(),
                    &pszStr, GDALExtendedDataType::CreateString());
                if (pszStr)
                {
                    poRAT->SetValue(iRow, iCol, pszStr);
                }
                CPLFree(pszStr);
            }
            iCol++;
        }
        oValuesDT.FreeDynamicMemory(&abyRow[0]);
    }
    return poRAT;
}
