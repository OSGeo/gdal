/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALNoDataMaskBand, a class implementing all
 *           a default band mask based on nodata values.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gdal_priv.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv_templates.hpp"

//! @cond Doxygen_Suppress
/************************************************************************/
/*                        GDALNoDataMaskBand()                          */
/************************************************************************/

GDALNoDataMaskBand::GDALNoDataMaskBand(GDALRasterBand *poParentIn)
    : m_poParent(poParentIn)
{
    poDS = nullptr;
    nBand = 0;

    nRasterXSize = m_poParent->GetXSize();
    nRasterYSize = m_poParent->GetYSize();

    eDataType = GDT_Byte;
    m_poParent->GetBlockSize(&nBlockXSize, &nBlockYSize);

    const auto eParentDT = m_poParent->GetRasterDataType();
    if (eParentDT == GDT_Int64)
        m_nNoDataValueInt64 = m_poParent->GetNoDataValueAsInt64();
    else if (eParentDT == GDT_UInt64)
        m_nNoDataValueUInt64 = m_poParent->GetNoDataValueAsUInt64();
    else
        m_dfNoDataValue = m_poParent->GetNoDataValue();
}

/************************************************************************/
/*                        GDALNoDataMaskBand()                          */
/************************************************************************/

GDALNoDataMaskBand::GDALNoDataMaskBand(GDALRasterBand *poParentIn,
                                       double dfNoDataValue)
    : m_poParent(poParentIn)
{
    poDS = nullptr;
    nBand = 0;

    nRasterXSize = m_poParent->GetXSize();
    nRasterYSize = m_poParent->GetYSize();

    eDataType = GDT_Byte;
    m_poParent->GetBlockSize(&nBlockXSize, &nBlockYSize);

    const auto eParentDT = m_poParent->GetRasterDataType();
    if (eParentDT == GDT_Int64)
        m_nNoDataValueInt64 = static_cast<int64_t>(dfNoDataValue);
    else if (eParentDT == GDT_UInt64)
        m_nNoDataValueUInt64 = static_cast<uint64_t>(dfNoDataValue);
    else
        m_dfNoDataValue = dfNoDataValue;
}

/************************************************************************/
/*                       ~GDALNoDataMaskBand()                          */
/************************************************************************/

GDALNoDataMaskBand::~GDALNoDataMaskBand() = default;

/************************************************************************/
/*                          GetWorkDataType()                           */
/************************************************************************/

static GDALDataType GetWorkDataType(GDALDataType eDataType)
{
    GDALDataType eWrkDT = GDT_Unknown;
    switch (eDataType)
    {
        case GDT_Byte:
            eWrkDT = GDT_Byte;
            break;

        case GDT_UInt16:
        case GDT_UInt32:
            eWrkDT = GDT_UInt32;
            break;

        case GDT_Int8:
        case GDT_Int16:
        case GDT_Int32:
        case GDT_CInt16:
        case GDT_CInt32:
            eWrkDT = GDT_Int32;
            break;

        case GDT_Float32:
        case GDT_CFloat32:
            eWrkDT = GDT_Float32;
            break;

        case GDT_Float64:
        case GDT_CFloat64:
            eWrkDT = GDT_Float64;
            break;

        case GDT_Int64:
        case GDT_UInt64:
            eWrkDT = eDataType;
            break;

        default:
            CPLAssert(false);
            eWrkDT = GDT_Float64;
            break;
    }
    return eWrkDT;
}

/************************************************************************/
/*                          IsNoDataInRange()                           */
/************************************************************************/

bool GDALNoDataMaskBand::IsNoDataInRange(double dfNoDataValue,
                                         GDALDataType eDataTypeIn)
{
    GDALDataType eWrkDT = GetWorkDataType(eDataTypeIn);
    switch (eWrkDT)
    {
        case GDT_Byte:
        {
            return GDALIsValueInRange<GByte>(dfNoDataValue);
        }

        case GDT_UInt32:
        {
            return GDALIsValueInRange<GUInt32>(dfNoDataValue);
        }
        case GDT_Int32:
        {
            return GDALIsValueInRange<GInt32>(dfNoDataValue);
        }

        case GDT_UInt64:
        {
            return GDALIsValueInRange<uint64_t>(dfNoDataValue);
        }

        case GDT_Int64:
        {
            return GDALIsValueInRange<int64_t>(dfNoDataValue);
        }

        case GDT_Float32:
        {
            return CPLIsNan(dfNoDataValue) || CPLIsInf(dfNoDataValue) ||
                   GDALIsValueInRange<float>(dfNoDataValue);
        }

        case GDT_Float64:
        {
            return true;
        }

        default:
            CPLAssert(false);
            return false;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALNoDataMaskBand::IReadBlock(int nXBlockOff, int nYBlockOff,
                                      void *pImage)

{
    const int nXOff = nXBlockOff * nBlockXSize;
    const int nXSizeRequest = std::min(nBlockXSize, nRasterXSize - nXOff);
    const int nYOff = nYBlockOff * nBlockYSize;
    const int nYSizeRequest = std::min(nBlockYSize, nRasterYSize - nYOff);

    if (nBlockXSize != nXSizeRequest || nBlockYSize != nYSizeRequest)
    {
        memset(pImage, 0, static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize);
    }

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    return IRasterIO(GF_Read, nXOff, nYOff, nXSizeRequest, nYSizeRequest,
                     pImage, nXSizeRequest, nYSizeRequest, GDT_Byte, 1,
                     nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALNoDataMaskBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                     int nXSize, int nYSize, void *pData,
                                     int nBufXSize, int nBufYSize,
                                     GDALDataType eBufType,
                                     GSpacing nPixelSpace, GSpacing nLineSpace,
                                     GDALRasterIOExtraArg *psExtraArg)
{
    if (eRWFlag != GF_Read)
    {
        return CE_Failure;
    }
    const auto eParentDT = m_poParent->GetRasterDataType();
    const GDALDataType eWrkDT = GetWorkDataType(eParentDT);

    // Optimization in common use case (#4488).
    // This avoids triggering the block cache on this band, which helps
    // reducing the global block cache consumption.
    if (eBufType == GDT_Byte && eWrkDT == GDT_Byte)
    {
        const CPLErr eErr = m_poParent->RasterIO(
            GF_Read, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg);
        if (eErr != CE_None)
            return eErr;

        GByte *pabyData = static_cast<GByte *>(pData);
        const GByte byNoData = static_cast<GByte>(m_dfNoDataValue);

        if (nPixelSpace == 1 && nLineSpace == nBufXSize)
        {
            const size_t nBufSize = static_cast<size_t>(nBufXSize) * nBufYSize;
            for (size_t i = 0; i < nBufSize; ++i)
            {
                pabyData[i] = pabyData[i] == byNoData ? 0 : 255;
            }
        }
        else
        {
            for (int iY = 0; iY < nBufYSize; iY++)
            {
                GByte *pabyLine = pabyData + iY * nLineSpace;
                for (int iX = 0; iX < nBufXSize; iX++)
                {
                    *pabyLine = *pabyLine == byNoData ? 0 : 255;
                    pabyLine += nPixelSpace;
                }
            }
        }
        return CE_None;
    }

    const auto AllocTempBufferOrFallback =
        [this, eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
         nBufYSize, eBufType, nPixelSpace, nLineSpace,
         psExtraArg](int nWrkDTSize) -> std::pair<CPLErr, void *>
    {
        auto poParentDS = m_poParent->GetDataset();
        // Check if we must simulate a memory allocation failure
        // Before checking the env variable, which is slightly expensive,
        // check first for a special dataset name, which is a cheap test.
        const char *pszOptVal =
            poParentDS && strcmp(poParentDS->GetDescription(), "__debug__") == 0
                ? CPLGetConfigOption(
                      "GDAL_SIMUL_MEM_ALLOC_FAILURE_NODATA_MASK_BAND", "NO")
                : "NO";
        const bool bSimulMemAllocFailure =
            EQUAL(pszOptVal, "ALWAYS") ||
            (CPLTestBool(pszOptVal) &&
             GDALMajorObject::GetMetadataItem(__func__, "__INTERNAL__") ==
                 nullptr);
        void *pTemp = nullptr;
        if (!bSimulMemAllocFailure)
        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            pTemp = VSI_MALLOC3_VERBOSE(nWrkDTSize, nBufXSize, nBufYSize);
        }
        if (!pTemp)
        {
            const bool bAllocHasAlreadyFailed =
                GDALMajorObject::GetMetadataItem(__func__, "__INTERNAL__") !=
                nullptr;
            CPLError(bAllocHasAlreadyFailed ? CE_Failure : CE_Warning,
                     CPLE_OutOfMemory,
                     "GDALNoDataMaskBand::IRasterIO(): cannot allocate %d x %d "
                     "x %d bytes%s",
                     nBufXSize, nBufYSize, nWrkDTSize,
                     bAllocHasAlreadyFailed
                         ? ""
                         : ". Falling back to block-based approach");
            if (bAllocHasAlreadyFailed)
                return std::pair(CE_Failure, nullptr);
            // Sets a metadata item to prevent potential infinite recursion
            GDALMajorObject::SetMetadataItem(__func__, "IN", "__INTERNAL__");
            const CPLErr eErr = GDALRasterBand::IRasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                nBufYSize, eBufType, nPixelSpace, nLineSpace, psExtraArg);
            GDALMajorObject::SetMetadataItem(__func__, nullptr, "__INTERNAL__");
            return std::pair(eErr, nullptr);
        }
        return std::pair(CE_None, pTemp);
    };

    if (eBufType == GDT_Byte)
    {
        const int nWrkDTSize = GDALGetDataTypeSizeBytes(eWrkDT);
        auto [eErr, pTemp] = AllocTempBufferOrFallback(nWrkDTSize);
        if (!pTemp)
            return eErr;

        eErr = m_poParent->RasterIO(
            GF_Read, nXOff, nYOff, nXSize, nYSize, pTemp, nBufXSize, nBufYSize,
            eWrkDT, nWrkDTSize, static_cast<GSpacing>(nBufXSize) * nWrkDTSize,
            psExtraArg);
        if (eErr != CE_None)
        {
            VSIFree(pTemp);
            return eErr;
        }

        const bool bIsNoDataNan = CPLIsNan(m_dfNoDataValue) != 0;
        GByte *pabyDest = static_cast<GByte *>(pData);

        /* --------------------------------------------------------------------
         */
        /*      Process different cases. */
        /* --------------------------------------------------------------------
         */
        switch (eWrkDT)
        {
            case GDT_UInt32:
            {
                const GUInt32 nNoData = static_cast<GUInt32>(m_dfNoDataValue);
                const GUInt32 *panSrc = static_cast<const GUInt32 *>(pTemp);

                size_t i = 0;
                for (int iY = 0; iY < nBufYSize; iY++)
                {
                    GByte *pabyLineDest = pabyDest + iY * nLineSpace;
                    for (int iX = 0; iX < nBufXSize; iX++)
                    {
                        *pabyLineDest = panSrc[i] == nNoData ? 0 : 255;
                        ++i;
                        pabyLineDest += nPixelSpace;
                    }
                }
            }
            break;

            case GDT_Int32:
            {
                const GInt32 nNoData = static_cast<GInt32>(m_dfNoDataValue);
                const GInt32 *panSrc = static_cast<const GInt32 *>(pTemp);

                size_t i = 0;
                for (int iY = 0; iY < nBufYSize; iY++)
                {
                    GByte *pabyLineDest = pabyDest + iY * nLineSpace;
                    for (int iX = 0; iX < nBufXSize; iX++)
                    {
                        *pabyLineDest = panSrc[i] == nNoData ? 0 : 255;
                        ++i;
                        pabyLineDest += nPixelSpace;
                    }
                }
            }
            break;

            case GDT_Float32:
            {
                const float fNoData = static_cast<float>(m_dfNoDataValue);
                const float *pafSrc = static_cast<const float *>(pTemp);

                size_t i = 0;
                for (int iY = 0; iY < nBufYSize; iY++)
                {
                    GByte *pabyLineDest = pabyDest + iY * nLineSpace;
                    for (int iX = 0; iX < nBufXSize; iX++)
                    {
                        const float fVal = pafSrc[i];
                        if (bIsNoDataNan && CPLIsNan(fVal))
                            *pabyLineDest = 0;
                        else if (ARE_REAL_EQUAL(fVal, fNoData))
                            *pabyLineDest = 0;
                        else
                            *pabyLineDest = 255;
                        ++i;
                        pabyLineDest += nPixelSpace;
                    }
                }
            }
            break;

            case GDT_Float64:
            {
                const double *padfSrc = static_cast<const double *>(pTemp);

                size_t i = 0;
                for (int iY = 0; iY < nBufYSize; iY++)
                {
                    GByte *pabyLineDest = pabyDest + iY * nLineSpace;
                    for (int iX = 0; iX < nBufXSize; iX++)
                    {
                        const double dfVal = padfSrc[i];
                        if (bIsNoDataNan && CPLIsNan(dfVal))
                            *pabyLineDest = 0;
                        else if (ARE_REAL_EQUAL(dfVal, m_dfNoDataValue))
                            *pabyLineDest = 0;
                        else
                            *pabyLineDest = 255;
                        ++i;
                        pabyLineDest += nPixelSpace;
                    }
                }
            }
            break;

            case GDT_Int64:
            {
                const auto *panSrc = static_cast<const int64_t *>(pTemp);

                size_t i = 0;
                for (int iY = 0; iY < nBufYSize; iY++)
                {
                    GByte *pabyLineDest = pabyDest + iY * nLineSpace;
                    for (int iX = 0; iX < nBufXSize; iX++)
                    {
                        const auto nVal = panSrc[i];
                        if (nVal == m_nNoDataValueInt64)
                            *pabyLineDest = 0;
                        else
                            *pabyLineDest = 255;
                        ++i;
                        pabyLineDest += nPixelSpace;
                    }
                }
            }
            break;

            case GDT_UInt64:
            {
                const auto *panSrc = static_cast<const uint64_t *>(pTemp);

                size_t i = 0;
                for (int iY = 0; iY < nBufYSize; iY++)
                {
                    GByte *pabyLineDest = pabyDest + iY * nLineSpace;
                    for (int iX = 0; iX < nBufXSize; iX++)
                    {
                        const auto nVal = panSrc[i];
                        if (nVal == m_nNoDataValueUInt64)
                            *pabyLineDest = 0;
                        else
                            *pabyLineDest = 255;
                        ++i;
                        pabyLineDest += nPixelSpace;
                    }
                }
            }
            break;

            default:
                CPLAssert(false);
                break;
        }

        VSIFree(pTemp);
        return CE_None;
    }

    // Output buffer is non-Byte. Ask for Byte and expand to user requested
    // type
    auto [eErr, pTemp] = AllocTempBufferOrFallback(sizeof(GByte));
    if (!pTemp)
        return eErr;

    eErr = IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pTemp, nBufXSize,
                     nBufYSize, GDT_Byte, 1, nBufXSize, psExtraArg);
    if (eErr != CE_None)
    {
        VSIFree(pTemp);
        return eErr;
    }

    for (int iY = 0; iY < nBufYSize; iY++)
    {
        GDALCopyWords(
            static_cast<GByte *>(pTemp) + static_cast<size_t>(iY) * nBufXSize,
            GDT_Byte, 1, static_cast<GByte *>(pData) + iY * nLineSpace,
            eBufType, static_cast<int>(nPixelSpace), nBufXSize);
    }
    VSIFree(pTemp);
    return CE_None;
}

//! @endcond
