/******************************************************************************
 *
 * Name:     gdalmultidim_array_regularly_spaced.cpp
 * Project:  GDAL Core
 * Purpose:  GDALMDArray::IsRegularlySpaced() and GDALMDArrayRegularlySpaced implementation
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"

#include <algorithm>

/************************************************************************/
/*                         IsRegularlySpaced()                          */
/************************************************************************/

/** Returns whether an array is a 1D regularly spaced array.
 *
 * @param[out] dfStart     First value in the array
 * @param[out] dfIncrement Increment/spacing between consecutive values.
 * @return true if the array is regularly spaced.
 */
bool GDALMDArray::IsRegularlySpaced(double &dfStart, double &dfIncrement) const
{
    dfStart = 0;
    dfIncrement = 0;
    if (GetDimensionCount() != 1 || GetDataType().GetClass() != GEDTC_NUMERIC)
        return false;
    const auto nSize = GetDimensions()[0]->GetSize();
    if (nSize <= 1 || nSize > 10 * 1000 * 1000)
        return false;

    size_t nCount = static_cast<size_t>(nSize);
    std::vector<double> adfTmp;
    try
    {
        adfTmp.resize(nCount);
    }
    catch (const std::exception &)
    {
        return false;
    }

    GUInt64 anStart[1] = {0};
    size_t anCount[1] = {nCount};

    const auto IsRegularlySpacedInternal =
        [&dfStart, &dfIncrement, &anCount, &adfTmp]()
    {
        dfStart = adfTmp[0];
        dfIncrement = (adfTmp[anCount[0] - 1] - adfTmp[0]) / (anCount[0] - 1);
        if (dfIncrement == 0)
        {
            return false;
        }
        for (size_t i = 1; i < anCount[0]; i++)
        {
            if (fabs((adfTmp[i] - adfTmp[i - 1]) - dfIncrement) >
                1e-3 * fabs(dfIncrement))
            {
                return false;
            }
        }
        return true;
    };

    // First try with the first block(s). This can avoid excessive processing
    // time, for example with Zarr datasets.
    // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=37636 and
    // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=39273
    const auto nBlockSize = GetBlockSize()[0];
    if (nCount >= 5 && nBlockSize <= nCount / 2)
    {
        size_t nReducedCount =
            std::max<size_t>(3, static_cast<size_t>(nBlockSize));
        while (nReducedCount < 256 && nReducedCount <= (nCount - 2) / 2)
            nReducedCount *= 2;
        anCount[0] = nReducedCount;
        if (!Read(anStart, anCount, nullptr, nullptr,
                  GDALExtendedDataType::Create(GDT_Float64), &adfTmp[0]))
        {
            return false;
        }
        if (!IsRegularlySpacedInternal())
        {
            return false;
        }

        // Get next values
        anStart[0] = nReducedCount;
        anCount[0] = nCount - nReducedCount;
    }

    if (!Read(anStart, anCount, nullptr, nullptr,
              GDALExtendedDataType::Create(GDT_Float64),
              &adfTmp[static_cast<size_t>(anStart[0])]))
    {
        return false;
    }

    return IsRegularlySpacedInternal();
}

//! @cond Doxygen_Suppress

GDALMDArrayRegularlySpaced::GDALMDArrayRegularlySpaced(
    const std::string &osParentName, const std::string &osName,
    const std::shared_ptr<GDALDimension> &poDim, double dfStart,
    double dfIncrement, double dfOffsetInIncrement)
    : GDALAbstractMDArray(osParentName, osName),
      GDALMDArray(osParentName, osName), m_dfStart(dfStart),
      m_dfIncrement(dfIncrement), m_dfOffsetInIncrement(dfOffsetInIncrement),
      m_dims{poDim}
{
}

std::shared_ptr<GDALMDArrayRegularlySpaced> GDALMDArrayRegularlySpaced::Create(
    const std::string &osParentName, const std::string &osName,
    const std::shared_ptr<GDALDimension> &poDim, double dfStart,
    double dfIncrement, double dfOffsetInIncrement)
{
    auto poArray = std::make_shared<GDALMDArrayRegularlySpaced>(
        osParentName, osName, poDim, dfStart, dfIncrement, dfOffsetInIncrement);
    poArray->SetSelf(poArray);
    return poArray;
}

const std::vector<std::shared_ptr<GDALDimension>> &
GDALMDArrayRegularlySpaced::GetDimensions() const
{
    return m_dims;
}

const GDALExtendedDataType &GDALMDArrayRegularlySpaced::GetDataType() const
{
    return m_dt;
}

std::vector<std::shared_ptr<GDALAttribute>>
GDALMDArrayRegularlySpaced::GetAttributes(CSLConstList) const
{
    return m_attributes;
}

void GDALMDArrayRegularlySpaced::AddAttribute(
    const std::shared_ptr<GDALAttribute> &poAttr)
{
    m_attributes.emplace_back(poAttr);
}

bool GDALMDArrayRegularlySpaced::IRead(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride, const GDALExtendedDataType &bufferDataType,
    void *pDstBuffer) const
{
    GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
    for (size_t i = 0; i < count[0]; i++)
    {
        const double dfVal =
            m_dfStart +
            (arrayStartIdx[0] + i * static_cast<double>(arrayStep[0]) +
             m_dfOffsetInIncrement) *
                m_dfIncrement;
        GDALExtendedDataType::CopyValue(&dfVal, m_dt, pabyDstBuffer,
                                        bufferDataType);
        pabyDstBuffer += bufferStride[0] * bufferDataType.GetSize();
    }
    return true;
}

bool GDALMDArrayRegularlySpaced::IsRegularlySpaced(double &dfStart,
                                                   double &dfIncrement) const
{
    dfStart = m_dfStart + m_dfOffsetInIncrement * m_dfIncrement;
    dfIncrement = m_dfIncrement;
    return true;
}

//! @endcond
