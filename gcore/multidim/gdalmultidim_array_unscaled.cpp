/******************************************************************************
 *
 * Name:     gdalmultidim_array_unscaled.cpp
 * Project:  GDAL Core
 * Purpose:  GDALMDArray::GetUnscaled() implementation
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdalmultidim_array_unscaled.h"

#include <cmath>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool GDALMDArrayUnscaled::IRead(const GUInt64 *arrayStartIdx,
                                const size_t *count, const GInt64 *arrayStep,
                                const GPtrDiff_t *bufferStride,
                                const GDALExtendedDataType &bufferDataType,
                                void *pDstBuffer) const
{
    const double dfScale = m_dfScale;
    const double dfOffset = m_dfOffset;
    const bool bDTIsComplex =
        CPL_TO_BOOL(GDALDataTypeIsComplex(m_dt.GetNumericDataType()));
    const auto dtDouble =
        GDALExtendedDataType::Create(bDTIsComplex ? GDT_CFloat64 : GDT_Float64);
    const size_t nDTSize = dtDouble.GetSize();
    const bool bTempBufferNeeded = (dtDouble != bufferDataType);

    double adfSrcNoData[2] = {0, 0};
    if (m_bHasNoData)
    {
        GDALExtendedDataType::CopyValue(m_poParent->GetRawNoDataValue(),
                                        m_poParent->GetDataType(),
                                        &adfSrcNoData[0], dtDouble);
    }

    const auto nDims = GetDimensions().size();
    if (nDims == 0)
    {
        double adfVal[2];
        if (!m_poParent->Read(arrayStartIdx, count, arrayStep, bufferStride,
                              dtDouble, &adfVal[0]))
        {
            return false;
        }
        if (!m_bHasNoData || adfVal[0] != adfSrcNoData[0])
        {
            adfVal[0] = adfVal[0] * dfScale + dfOffset;
            if (bDTIsComplex)
            {
                adfVal[1] = adfVal[1] * dfScale + dfOffset;
            }
            GDALExtendedDataType::CopyValue(&adfVal[0], dtDouble, pDstBuffer,
                                            bufferDataType);
        }
        else
        {
            GDALExtendedDataType::CopyValue(m_abyRawNoData.data(), m_dt,
                                            pDstBuffer, bufferDataType);
        }
        return true;
    }

    std::vector<GPtrDiff_t> actualBufferStrideVector;
    const GPtrDiff_t *actualBufferStridePtr = bufferStride;
    void *pTempBuffer = pDstBuffer;
    if (bTempBufferNeeded)
    {
        size_t nElts = 1;
        actualBufferStrideVector.resize(nDims);
        for (size_t i = 0; i < nDims; i++)
            nElts *= count[i];
        actualBufferStrideVector.back() = 1;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            actualBufferStrideVector[i] =
                actualBufferStrideVector[i + 1] * count[i + 1];
        }
        actualBufferStridePtr = actualBufferStrideVector.data();
        pTempBuffer = VSI_MALLOC2_VERBOSE(nDTSize, nElts);
        if (!pTempBuffer)
            return false;
    }
    if (!m_poParent->Read(arrayStartIdx, count, arrayStep,
                          actualBufferStridePtr, dtDouble, pTempBuffer))
    {
        if (bTempBufferNeeded)
            VSIFree(pTempBuffer);
        return false;
    }

    struct Stack
    {
        size_t nIters = 0;
        double *src_ptr = nullptr;
        GByte *dst_ptr = nullptr;
        GPtrDiff_t src_inc_offset = 0;
        GPtrDiff_t dst_inc_offset = 0;
    };

    std::vector<Stack> stack(nDims);
    const size_t nBufferDTSize = bufferDataType.GetSize();
    for (size_t i = 0; i < nDims; i++)
    {
        stack[i].src_inc_offset =
            actualBufferStridePtr[i] * (bDTIsComplex ? 2 : 1);
        stack[i].dst_inc_offset =
            static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
    }
    stack[0].src_ptr = static_cast<double *>(pTempBuffer);
    stack[0].dst_ptr = static_cast<GByte *>(pDstBuffer);

    size_t dimIdx = 0;
    const size_t nDimsMinus1 = nDims - 1;
    GByte abyDstNoData[16];
    CPLAssert(nBufferDTSize <= sizeof(abyDstNoData));
    GDALExtendedDataType::CopyValue(m_abyRawNoData.data(), m_dt, abyDstNoData,
                                    bufferDataType);

lbl_next_depth:
    if (dimIdx == nDimsMinus1)
    {
        auto nIters = count[dimIdx];
        double *padfVal = stack[dimIdx].src_ptr;
        GByte *dst_ptr = stack[dimIdx].dst_ptr;
        while (true)
        {
            if (!m_bHasNoData || padfVal[0] != adfSrcNoData[0])
            {
                padfVal[0] = padfVal[0] * dfScale + dfOffset;
                if (bDTIsComplex)
                {
                    padfVal[1] = padfVal[1] * dfScale + dfOffset;
                }
                if (bTempBufferNeeded)
                {
                    GDALExtendedDataType::CopyValue(&padfVal[0], dtDouble,
                                                    dst_ptr, bufferDataType);
                }
            }
            else
            {
                memcpy(dst_ptr, abyDstNoData, nBufferDTSize);
            }

            if ((--nIters) == 0)
                break;
            padfVal += stack[dimIdx].src_inc_offset;
            dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    else
    {
        stack[dimIdx].nIters = count[dimIdx];
        while (true)
        {
            dimIdx++;
            stack[dimIdx].src_ptr = stack[dimIdx - 1].src_ptr;
            stack[dimIdx].dst_ptr = stack[dimIdx - 1].dst_ptr;
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if ((--stack[dimIdx].nIters) == 0)
                break;
            stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    if (bTempBufferNeeded)
        VSIFree(pTempBuffer);
    return true;
}

/************************************************************************/
/*                               IWrite()                               */
/************************************************************************/

bool GDALMDArrayUnscaled::IWrite(const GUInt64 *arrayStartIdx,
                                 const size_t *count, const GInt64 *arrayStep,
                                 const GPtrDiff_t *bufferStride,
                                 const GDALExtendedDataType &bufferDataType,
                                 const void *pSrcBuffer)
{
    const double dfScale = m_dfScale;
    const double dfOffset = m_dfOffset;
    const bool bDTIsComplex =
        CPL_TO_BOOL(GDALDataTypeIsComplex(m_dt.GetNumericDataType()));
    const auto dtDouble =
        GDALExtendedDataType::Create(bDTIsComplex ? GDT_CFloat64 : GDT_Float64);
    const size_t nDTSize = dtDouble.GetSize();
    const bool bIsBufferDataTypeNativeDataType = (dtDouble == bufferDataType);
    const bool bSelfAndParentHaveNoData =
        m_bHasNoData && m_poParent->GetRawNoDataValue() != nullptr;
    double dfNoData = 0;
    if (m_bHasNoData)
    {
        GDALCopyWords64(m_abyRawNoData.data(), m_dt.GetNumericDataType(), 0,
                        &dfNoData, GDT_Float64, 0, 1);
    }

    double adfSrcNoData[2] = {0, 0};
    if (bSelfAndParentHaveNoData)
    {
        GDALExtendedDataType::CopyValue(m_poParent->GetRawNoDataValue(),
                                        m_poParent->GetDataType(),
                                        &adfSrcNoData[0], dtDouble);
    }

    const auto nDims = GetDimensions().size();
    if (nDims == 0)
    {
        double adfVal[2];
        GDALExtendedDataType::CopyValue(pSrcBuffer, bufferDataType, &adfVal[0],
                                        dtDouble);
        if (bSelfAndParentHaveNoData &&
            (std::isnan(adfVal[0]) || adfVal[0] == dfNoData))
        {
            return m_poParent->Write(arrayStartIdx, count, arrayStep,
                                     bufferStride, m_poParent->GetDataType(),
                                     m_poParent->GetRawNoDataValue());
        }
        else
        {
            adfVal[0] = (adfVal[0] - dfOffset) / dfScale;
            if (bDTIsComplex)
            {
                adfVal[1] = (adfVal[1] - dfOffset) / dfScale;
            }
            return m_poParent->Write(arrayStartIdx, count, arrayStep,
                                     bufferStride, dtDouble, &adfVal[0]);
        }
    }

    std::vector<GPtrDiff_t> tmpBufferStrideVector;
    size_t nElts = 1;
    tmpBufferStrideVector.resize(nDims);
    for (size_t i = 0; i < nDims; i++)
        nElts *= count[i];
    tmpBufferStrideVector.back() = 1;
    for (size_t i = nDims - 1; i > 0;)
    {
        --i;
        tmpBufferStrideVector[i] = tmpBufferStrideVector[i + 1] * count[i + 1];
    }
    const GPtrDiff_t *tmpBufferStridePtr = tmpBufferStrideVector.data();
    void *pTempBuffer = VSI_MALLOC2_VERBOSE(nDTSize, nElts);
    if (!pTempBuffer)
        return false;

    struct Stack
    {
        size_t nIters = 0;
        double *dst_ptr = nullptr;
        const GByte *src_ptr = nullptr;
        GPtrDiff_t src_inc_offset = 0;
        GPtrDiff_t dst_inc_offset = 0;
    };

    std::vector<Stack> stack(nDims);
    const size_t nBufferDTSize = bufferDataType.GetSize();
    for (size_t i = 0; i < nDims; i++)
    {
        stack[i].dst_inc_offset =
            tmpBufferStridePtr[i] * (bDTIsComplex ? 2 : 1);
        stack[i].src_inc_offset =
            static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
    }
    stack[0].dst_ptr = static_cast<double *>(pTempBuffer);
    stack[0].src_ptr = static_cast<const GByte *>(pSrcBuffer);

    size_t dimIdx = 0;
    const size_t nDimsMinus1 = nDims - 1;

lbl_next_depth:
    if (dimIdx == nDimsMinus1)
    {
        auto nIters = count[dimIdx];
        double *dst_ptr = stack[dimIdx].dst_ptr;
        const GByte *src_ptr = stack[dimIdx].src_ptr;
        while (true)
        {
            double adfVal[2];
            const double *padfSrcVal;
            if (bIsBufferDataTypeNativeDataType)
            {
                padfSrcVal = reinterpret_cast<const double *>(src_ptr);
            }
            else
            {
                GDALExtendedDataType::CopyValue(src_ptr, bufferDataType,
                                                &adfVal[0], dtDouble);
                padfSrcVal = adfVal;
            }

            if (bSelfAndParentHaveNoData &&
                (std::isnan(padfSrcVal[0]) || padfSrcVal[0] == dfNoData))
            {
                dst_ptr[0] = adfSrcNoData[0];
                if (bDTIsComplex)
                {
                    dst_ptr[1] = adfSrcNoData[1];
                }
            }
            else
            {
                dst_ptr[0] = (padfSrcVal[0] - dfOffset) / dfScale;
                if (bDTIsComplex)
                {
                    dst_ptr[1] = (padfSrcVal[1] - dfOffset) / dfScale;
                }
            }

            if ((--nIters) == 0)
                break;
            dst_ptr += stack[dimIdx].dst_inc_offset;
            src_ptr += stack[dimIdx].src_inc_offset;
        }
    }
    else
    {
        stack[dimIdx].nIters = count[dimIdx];
        while (true)
        {
            dimIdx++;
            stack[dimIdx].src_ptr = stack[dimIdx - 1].src_ptr;
            stack[dimIdx].dst_ptr = stack[dimIdx - 1].dst_ptr;
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if ((--stack[dimIdx].nIters) == 0)
                break;
            stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    // If the parent array is not double/complex-double, then convert the
    // values to it, before calling Write(), as some implementations can be
    // very slow when doing the type conversion.
    const auto &eParentDT = m_poParent->GetDataType();
    const size_t nParentDTSize = eParentDT.GetSize();
    if (nParentDTSize <= nDTSize / 2)
    {
        // Copy in-place by making sure that source and target do not overlap
        const auto eNumericDT = dtDouble.GetNumericDataType();
        const auto eParentNumericDT = eParentDT.GetNumericDataType();

        // Copy first element
        {
            std::vector<GByte> abyTemp(nParentDTSize);
            GDALCopyWords64(static_cast<GByte *>(pTempBuffer), eNumericDT,
                            static_cast<int>(nDTSize), &abyTemp[0],
                            eParentNumericDT, static_cast<int>(nParentDTSize),
                            1);
            memcpy(pTempBuffer, abyTemp.data(), abyTemp.size());
        }
        // Remaining elements
        for (size_t i = 1; i < nElts; ++i)
        {
            GDALCopyWords64(
                static_cast<GByte *>(pTempBuffer) + i * nDTSize, eNumericDT, 0,
                static_cast<GByte *>(pTempBuffer) + i * nParentDTSize,
                eParentNumericDT, 0, 1);
        }
    }

    const bool ret =
        m_poParent->Write(arrayStartIdx, count, arrayStep, tmpBufferStridePtr,
                          eParentDT, pTempBuffer);

    VSIFree(pTempBuffer);
    return ret;
}

//! @endcond

/************************************************************************/
/*                            GetUnscaled()                             */
/************************************************************************/

/** Return an array that is the unscaled version of the current one.
 *
 * That is each value of the unscaled array will be
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * Starting with GDAL 3.3, the Write() method is implemented and will convert
 * from unscaled values to raw values.
 *
 * This is the same as the C function GDALMDArrayGetUnscaled().
 *
 * @param dfOverriddenScale Custom scale value instead of GetScale()
 * @param dfOverriddenOffset Custom offset value instead of GetOffset()
 * @param dfOverriddenDstNodata Custom target nodata value instead of NaN
 * @return a new array, that holds a reference to the original one, and thus is
 * a view of it (not a copy), or nullptr in case of error.
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::GetUnscaled(double dfOverriddenScale, double dfOverriddenOffset,
                         double dfOverriddenDstNodata) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if (!self)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    if (GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetUnscaled() only supports numeric data type");
        return nullptr;
    }
    const double dfScale =
        std::isnan(dfOverriddenScale) ? GetScale() : dfOverriddenScale;
    const double dfOffset =
        std::isnan(dfOverriddenOffset) ? GetOffset() : dfOverriddenOffset;
    if (dfScale == 1.0 && dfOffset == 0.0)
        return self;

    GDALDataType eDT = GDALDataTypeIsComplex(GetDataType().GetNumericDataType())
                           ? GDT_CFloat64
                           : GDT_Float64;
    if (dfOverriddenScale == -1 && dfOverriddenOffset == 0)
    {
        if (GetDataType().GetNumericDataType() == GDT_Float16)
            eDT = GDT_Float16;
        if (GetDataType().GetNumericDataType() == GDT_Float32)
            eDT = GDT_Float32;
    }

    return GDALMDArrayUnscaled::Create(self, dfScale, dfOffset,
                                       dfOverriddenDstNodata, eDT);
}
