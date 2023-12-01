/******************************************************************************
 * Name:     gdalmultidim_rat.cpp
 * Project:  GDAL Core
 * Purpose:  GDALCreateRasterAttributeTableFromMDArrays() implementation
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_priv.h"
#include "gdal_rat.h"
#include "gdalmultidim_priv.h"

/************************************************************************/
/*                 GDALRasterAttributeTableFromMDArrays()               */
/************************************************************************/

class GDALRasterAttributeTableFromMDArrays final
    : public GDALRasterAttributeTable
{
    const GDALRATTableType m_eTableType;
    const std::vector<std::shared_ptr<GDALMDArray>> m_apoArrays;
    const std::vector<GDALRATFieldUsage> m_aeUsages;

    mutable std::string m_osTmp{};

  public:
    GDALRasterAttributeTableFromMDArrays(
        GDALRATTableType eTableType,
        const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
        const std::vector<GDALRATFieldUsage> &aeUsages);

    //
    GDALRasterAttributeTable *Clone() const override
    {
        return new GDALRasterAttributeTableFromMDArrays(
            m_eTableType, m_apoArrays, m_aeUsages);
    }

    //
    int GetColumnCount() const override
    {
        return static_cast<int>(m_apoArrays.size());
    }

    const char *GetNameOfCol(int iCol) const override
    {
        if (iCol < 0 || iCol >= GetColumnCount())
            return nullptr;
        return m_apoArrays[iCol]->GetName().c_str();
    }

    //
    GDALRATFieldUsage GetUsageOfCol(int iCol) const override
    {
        if (iCol < 0 || iCol >= GetColumnCount() || m_aeUsages.empty())
            return GFU_Generic;
        return m_aeUsages[iCol];
    }

    //
    GDALRATFieldType GetTypeOfCol(int iCol) const override
    {
        if (iCol < 0 || iCol >= GetColumnCount())
            return GFT_Integer;
        switch (m_apoArrays[iCol]->GetDataType().GetNumericDataType())
        {
            case GDT_Int8:
            case GDT_Byte:
            case GDT_UInt16:
            case GDT_Int16:
            case GDT_Int32:
                return GFT_Integer;
            case GDT_UInt32:
            case GDT_Int64:
            case GDT_UInt64:
            case GDT_Float32:
            case GDT_Float64:
                return GFT_Real;
            default:
                break;
        }
        return GFT_String;
    }

    //
    int GetColOfUsage(GDALRATFieldUsage eUsage) const override
    {
        const int nColCount = GetColumnCount();
        for (int i = 0; i < nColCount; i++)
        {
            if (GetUsageOfCol(i) == eUsage)
                return i;
        }

        return -1;
    }

    //
    int GetRowCount() const override
    {
        return static_cast<int>(m_apoArrays[0]->GetDimensions()[0]->GetSize());
    }

    //
    const char *GetValueAsString(int iRow, int iField) const override
    {
        if (iRow < 0 || iRow >= GetRowCount() || iField < 0 ||
            iField >= GetColumnCount())
            return nullptr;

        const GUInt64 arrayStartIdx[1] = {static_cast<GUInt64>(iRow)};
        const size_t count[1] = {1};
        const GInt64 arrayStep[1] = {1};
        const GPtrDiff_t bufferStride[1] = {1};
        char *pszStr = nullptr;
        void *pDstBuffer = &pszStr;
        if (!m_apoArrays[iField]->Read(
                arrayStartIdx, count, arrayStep, bufferStride,
                GDALExtendedDataType::CreateString(), pDstBuffer))
            return nullptr;
        if (!pszStr)
            return nullptr;
        m_osTmp = pszStr;
        CPLFree(pszStr);
        return m_osTmp.c_str();
    }

    //
    int GetValueAsInt(int iRow, int iField) const override
    {
        if (iRow < 0 || iRow >= GetRowCount() || iField < 0 ||
            iField >= GetColumnCount())
            return 0;

        const GUInt64 arrayStartIdx[1] = {static_cast<GUInt64>(iRow)};
        const size_t count[1] = {1};
        const GInt64 arrayStep[1] = {1};
        const GPtrDiff_t bufferStride[1] = {1};
        int nVal = 0;
        void *pDstBuffer = &nVal;
        if (!m_apoArrays[iField]->Read(
                arrayStartIdx, count, arrayStep, bufferStride,
                GDALExtendedDataType::Create(GDT_Int32), pDstBuffer))
            return 0;
        return nVal;
    }

    //
    double GetValueAsDouble(int iRow, int iField) const override
    {
        if (iRow < 0 || iRow >= GetRowCount() || iField < 0 ||
            iField >= GetColumnCount())
            return 0;

        const GUInt64 arrayStartIdx[1] = {static_cast<GUInt64>(iRow)};
        const size_t count[1] = {1};
        const GInt64 arrayStep[1] = {1};
        const GPtrDiff_t bufferStride[1] = {1};
        double dfVal = 0;
        void *pDstBuffer = &dfVal;
        if (!m_apoArrays[iField]->Read(
                arrayStartIdx, count, arrayStep, bufferStride,
                GDALExtendedDataType::Create(GDT_Float64), pDstBuffer))
            return 0;
        return dfVal;
    }

    //
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    double *pdfData) override
    {
        if (eRWFlag != GF_Read)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "GDALRasterAttributeTableFromMDArrays::ValuesIO(): "
                     "eRWFlag != GF_Read not supported");
            return CE_Failure;
        }
        if (iStartRow < 0 || iLength <= 0 ||
            iStartRow > GetRowCount() - iLength)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid iStartRow/iLength");
            return CE_Failure;
        }
        if (iField < 0 || iField >= GetColumnCount())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid iField");
            return CE_Failure;
        }
        const GUInt64 arrayStartIdx[1] = {static_cast<GUInt64>(iStartRow)};
        const size_t count[1] = {static_cast<size_t>(iLength)};
        const GInt64 arrayStep[1] = {1};
        const GPtrDiff_t bufferStride[1] = {1};
        if (!m_apoArrays[iField]->Read(
                arrayStartIdx, count, arrayStep, bufferStride,
                GDALExtendedDataType::Create(GDT_Float64), pdfData))
            return CE_Failure;
        return CE_None;
    }

    //
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    int *pnData) override
    {
        if (eRWFlag != GF_Read)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "GDALRasterAttributeTableFromMDArrays::ValuesIO(): "
                     "eRWFlag != GF_Read not supported");
            return CE_Failure;
        }
        if (iStartRow < 0 || iLength <= 0 ||
            iStartRow > GetRowCount() - iLength)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid iStartRow/iLength");
            return CE_Failure;
        }
        if (iField < 0 || iField >= GetColumnCount())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid iField");
            return CE_Failure;
        }
        const GUInt64 arrayStartIdx[1] = {static_cast<GUInt64>(iStartRow)};
        const size_t count[1] = {static_cast<size_t>(iLength)};
        const GInt64 arrayStep[1] = {1};
        const GPtrDiff_t bufferStride[1] = {1};
        if (!m_apoArrays[iField]->Read(
                arrayStartIdx, count, arrayStep, bufferStride,
                GDALExtendedDataType::Create(GDT_Int32), pnData))
            return CE_Failure;
        return CE_None;
    }

    //
    CPLErr ValuesIO(GDALRWFlag eRWFlag, int iField, int iStartRow, int iLength,
                    char **papszStrList) override
    {
        if (eRWFlag != GF_Read)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "GDALRasterAttributeTableFromMDArrays::ValuesIO(): "
                     "eRWFlag != GF_Read not supported");
            return CE_Failure;
        }
        if (iStartRow < 0 || iLength <= 0 ||
            iStartRow > GetRowCount() - iLength)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid iStartRow/iLength");
            return CE_Failure;
        }
        if (iField < 0 || iField >= GetColumnCount())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid iField");
            return CE_Failure;
        }
        const GUInt64 arrayStartIdx[1] = {static_cast<GUInt64>(iStartRow)};
        const size_t count[1] = {static_cast<size_t>(iLength)};
        const GInt64 arrayStep[1] = {1};
        const GPtrDiff_t bufferStride[1] = {1};
        if (!m_apoArrays[iField]->Read(
                arrayStartIdx, count, arrayStep, bufferStride,
                GDALExtendedDataType::CreateString(), papszStrList))
            return CE_Failure;
        return CE_None;
    }

    //
    void SetValue(int, int, const char *) override
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "GDALRasterAttributeTableFromMDArrays::SetValue(): not supported");
    }

    //
    void SetValue(int, int, int) override
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "GDALRasterAttributeTableFromMDArrays::SetValue(): not supported");
    }

    //
    void SetValue(int, int, double) override
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "GDALRasterAttributeTableFromMDArrays::SetValue(): not supported");
    }

    //
    int ChangesAreWrittenToFile() override
    {
        return false;
    }

    //
    CPLErr SetTableType(const GDALRATTableType) override
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRasterAttributeTableFromMDArrays::SetTableType(): not "
                 "supported");
        return CE_Failure;
    }

    //
    void RemoveStatistics() override
    {
    }

    //
    GDALRATTableType GetTableType() const override
    {
        return m_eTableType;
    }
};

/************************************************************************/
/*               GDALRasterAttributeTableFromMDArrays()                 */
/************************************************************************/

GDALRasterAttributeTableFromMDArrays::GDALRasterAttributeTableFromMDArrays(
    GDALRATTableType eTableType,
    const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
    const std::vector<GDALRATFieldUsage> &aeUsages)
    : m_eTableType(eTableType), m_apoArrays(apoArrays), m_aeUsages(aeUsages)
{
}

/************************************************************************/
/*             GDALCreateRasterAttributeTableFromMDArrays()             */
/************************************************************************/

/** Return a virtual Raster Attribute Table from several GDALMDArray's.
 *
 * All arrays must be single-dimensional and be indexed by the same dimension.
 *
 * This is the same as the C function GDALCreateRasterAttributeTableFromMDArrays().
 *
 * @param eTableType RAT table type
 * @param apoArrays Vector of GDALMDArray's (none of them should be nullptr)
 * @param aeUsages Vector of GDALRATFieldUsage (of the same size as apoArrays if non-empty), or empty vector to use defaults
 * @return a new Raster Attribute Table to free with delete, or nullptr in case of error
 * @since 3.9
 */
GDALRasterAttributeTable *GDALCreateRasterAttributeTableFromMDArrays(
    GDALRATTableType eTableType,
    const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
    const std::vector<GDALRATFieldUsage> &aeUsages)
{
    if (apoArrays.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALCreateRasterAttributeTableFromMDArrays(): apoArrays "
                 "should not be empty");
        return nullptr;
    }
    if (!aeUsages.empty() && apoArrays.size() != aeUsages.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALCreateRasterAttributeTableFromMDArrays(): aeUsages "
                 "should be empty or have the same size as apoArrays");
        return nullptr;
    }
    for (size_t i = 0; i < apoArrays.size(); ++i)
    {
        if (apoArrays[i]->GetDimensionCount() != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALCreateRasterAttributeTableFromMDArrays(): "
                     "apoArrays[%d] has a dimension count != 1",
                     static_cast<int>(i));
            return nullptr;
        }
        if (i > 0 && (apoArrays[i]->GetDimensions()[0]->GetFullName() !=
                          apoArrays[0]->GetDimensions()[0]->GetFullName() ||
                      apoArrays[i]->GetDimensions()[0]->GetSize() !=
                          apoArrays[0]->GetDimensions()[0]->GetSize()))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "GDALCreateRasterAttributeTableFromMDArrays(): apoArrays[%d] "
                "does not have the same dimension has apoArrays[0]",
                static_cast<int>(i));
            return nullptr;
        }
    }
    return new GDALRasterAttributeTableFromMDArrays(eTableType, apoArrays,
                                                    aeUsages);
}

/************************************************************************/
/*             GDALCreateRasterAttributeTableFromMDArrays()             */
/************************************************************************/

/** Return a virtual Raster Attribute Table from several GDALMDArray's.
 *
 * All arrays must be single-dimensional and be indexed by the same dimension.
 *
 * This is the same as the C++ method GDALCreateRasterAttributeTableFromMDArrays().
 *
 * @param eTableType RAT table type
 * @param nArrays Number of elements in ahArrays parameter
 * @param ahArrays Array of nArrays GDALMDArray's (none of them should be nullptr)
 * @param paeUsages Array of nArray GDALRATFieldUsage, or nullptr to use defaults
 * @return a new Raster Attribute Table to free with GDALDestroyRasterAttributeTable(), or nullptr in case of error
 * @since 3.9
 */
GDALRasterAttributeTableH GDALCreateRasterAttributeTableFromMDArrays(
    GDALRATTableType eTableType, int nArrays, const GDALMDArrayH *ahArrays,
    const GDALRATFieldUsage *paeUsages)
{
    VALIDATE_POINTER1(ahArrays, __func__, nullptr);
    std::vector<std::shared_ptr<GDALMDArray>> apoArrays;
    std::vector<GDALRATFieldUsage> aeUsages;
    for (int i = 0; i < nArrays; ++i)
    {
        VALIDATE_POINTER1(ahArrays[i], __func__, nullptr);
        apoArrays.emplace_back(ahArrays[i]->m_poImpl);
        if (paeUsages)
            aeUsages.emplace_back(paeUsages[i]);
    }
    return GDALRasterAttributeTable::ToHandle(
        GDALCreateRasterAttributeTableFromMDArrays(eTableType, apoArrays,
                                                   aeUsages));
}
