/******************************************************************************
 *
 * Name:     gdalmultidim_array_mask.cpp
 * Project:  GDAL Core
 * Purpose:  GDALMDArray::GetMask() implementation
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_float.h"
#include "gdal_multidim.h"
#include "gdal_pam_multidim.h"

#include <algorithm>
#include <cmath>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                           GDALMDArrayMask                            */
/************************************************************************/

class GDALMDArrayMask final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    GDALExtendedDataType m_dt{GDALExtendedDataType::Create(GDT_UInt8)};
    double m_dfMissingValue = 0.0;
    bool m_bHasMissingValue = false;
    double m_dfFillValue = 0.0;
    bool m_bHasFillValue = false;
    double m_dfValidMin = 0.0;
    bool m_bHasValidMin = false;
    double m_dfValidMax = 0.0;
    bool m_bHasValidMax = false;
    std::vector<uint32_t> m_anValidFlagMasks{};
    std::vector<uint32_t> m_anValidFlagValues{};

    bool Init(CSLConstList papszOptions);

    template <typename Type>
    void
    ReadInternal(const size_t *count, const GPtrDiff_t *bufferStride,
                 const GDALExtendedDataType &bufferDataType, void *pDstBuffer,
                 const void *pTempBuffer,
                 const GDALExtendedDataType &oTmpBufferDT,
                 const std::vector<GPtrDiff_t> &tmpBufferStrideVector) const;

  protected:
    explicit GDALMDArrayMask(const std::shared_ptr<GDALMDArray> &poParent)
        : GDALAbstractMDArray(std::string(),
                              "Mask of " + poParent->GetFullName()),
          GDALPamMDArray(std::string(), "Mask of " + poParent->GetFullName(),
                         GDALPamMultiDim::GetPAM(poParent),
                         poParent->GetContext()),
          m_poParent(std::move(poParent))
    {
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override
    {
        return m_poParent->AdviseRead(arrayStartIdx, count, papszOptions);
    }

  public:
    static std::shared_ptr<GDALMDArrayMask>
    Create(const std::shared_ptr<GDALMDArray> &poParent,
           CSLConstList papszOptions);

    bool IsWritable() const override
    {
        return false;
    }

    const std::string &GetFilename() const override
    {
        return m_poParent->GetFilename();
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_poParent->GetDimensions();
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        return m_poParent->GetSpatialRef();
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        return m_poParent->GetBlockSize();
    }
};

/************************************************************************/
/*                      GDALMDArrayMask::Create()                       */
/************************************************************************/

/* static */ std::shared_ptr<GDALMDArrayMask>
GDALMDArrayMask::Create(const std::shared_ptr<GDALMDArray> &poParent,
                        CSLConstList papszOptions)
{
    auto newAr(std::shared_ptr<GDALMDArrayMask>(new GDALMDArrayMask(poParent)));
    newAr->SetSelf(newAr);
    if (!newAr->Init(papszOptions))
        return nullptr;
    return newAr;
}

/************************************************************************/
/*                       GDALMDArrayMask::Init()                        */
/************************************************************************/

bool GDALMDArrayMask::Init(CSLConstList papszOptions)
{
    const auto GetSingleValNumericAttr =
        [this](const char *pszAttrName, bool &bHasVal, double &dfVal)
    {
        auto poAttr = m_poParent->GetAttribute(pszAttrName);
        if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            const auto anDimSizes = poAttr->GetDimensionsSize();
            if (anDimSizes.empty() ||
                (anDimSizes.size() == 1 && anDimSizes[0] == 1))
            {
                bHasVal = true;
                dfVal = poAttr->ReadAsDouble();
            }
        }
    };

    GetSingleValNumericAttr("missing_value", m_bHasMissingValue,
                            m_dfMissingValue);
    GetSingleValNumericAttr("_FillValue", m_bHasFillValue, m_dfFillValue);
    GetSingleValNumericAttr("valid_min", m_bHasValidMin, m_dfValidMin);
    GetSingleValNumericAttr("valid_max", m_bHasValidMax, m_dfValidMax);

    {
        auto poValidRange = m_poParent->GetAttribute("valid_range");
        if (poValidRange && poValidRange->GetDimensionsSize().size() == 1 &&
            poValidRange->GetDimensionsSize()[0] == 2 &&
            poValidRange->GetDataType().GetClass() == GEDTC_NUMERIC)
        {
            m_bHasValidMin = true;
            m_bHasValidMax = true;
            auto vals = poValidRange->ReadAsDoubleArray();
            CPLAssert(vals.size() == 2);
            m_dfValidMin = vals[0];
            m_dfValidMax = vals[1];
        }
    }

    // Take into account
    // https://cfconventions.org/cf-conventions/cf-conventions.html#flags
    // Cf GDALMDArray::GetMask() for semantics of UNMASK_FLAGS
    const char *pszUnmaskFlags =
        CSLFetchNameValue(papszOptions, "UNMASK_FLAGS");
    if (pszUnmaskFlags)
    {
        const auto IsScalarStringAttr =
            [](const std::shared_ptr<GDALAttribute> &poAttr)
        {
            return poAttr->GetDataType().GetClass() == GEDTC_STRING &&
                   (poAttr->GetDimensionsSize().empty() ||
                    (poAttr->GetDimensionsSize().size() == 1 &&
                     poAttr->GetDimensionsSize()[0] == 1));
        };

        auto poFlagMeanings = m_poParent->GetAttribute("flag_meanings");
        if (!(poFlagMeanings && IsScalarStringAttr(poFlagMeanings)))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "UNMASK_FLAGS option specified but array has no "
                     "flag_meanings attribute");
            return false;
        }
        const char *pszFlagMeanings = poFlagMeanings->ReadAsString();
        if (!pszFlagMeanings)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot read flag_meanings attribute");
            return false;
        }

        const auto IsSingleDimNumericAttr =
            [](const std::shared_ptr<GDALAttribute> &poAttr)
        {
            return poAttr->GetDataType().GetClass() == GEDTC_NUMERIC &&
                   poAttr->GetDimensionsSize().size() == 1;
        };

        auto poFlagValues = m_poParent->GetAttribute("flag_values");
        const bool bHasFlagValues =
            poFlagValues && IsSingleDimNumericAttr(poFlagValues);

        auto poFlagMasks = m_poParent->GetAttribute("flag_masks");
        const bool bHasFlagMasks =
            poFlagMasks && IsSingleDimNumericAttr(poFlagMasks);

        if (!bHasFlagValues && !bHasFlagMasks)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find flag_values and/or flag_masks attribute");
            return false;
        }

        const CPLStringList aosUnmaskFlags(
            CSLTokenizeString2(pszUnmaskFlags, ",", 0));
        const CPLStringList aosFlagMeanings(
            CSLTokenizeString2(pszFlagMeanings, " ", 0));

        if (bHasFlagValues)
        {
            const auto eType = poFlagValues->GetDataType().GetNumericDataType();
            // We could support Int64 or UInt64, but more work...
            if (eType != GDT_UInt8 && eType != GDT_Int8 &&
                eType != GDT_UInt16 && eType != GDT_Int16 &&
                eType != GDT_UInt32 && eType != GDT_Int32)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported data type for flag_values attribute: %s",
                         GDALGetDataTypeName(eType));
                return false;
            }
        }

        if (bHasFlagMasks)
        {
            const auto eType = poFlagMasks->GetDataType().GetNumericDataType();
            // We could support Int64 or UInt64, but more work...
            if (eType != GDT_UInt8 && eType != GDT_Int8 &&
                eType != GDT_UInt16 && eType != GDT_Int16 &&
                eType != GDT_UInt32 && eType != GDT_Int32)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported data type for flag_masks attribute: %s",
                         GDALGetDataTypeName(eType));
                return false;
            }
        }

        const std::vector<double> adfValues(
            bHasFlagValues ? poFlagValues->ReadAsDoubleArray()
                           : std::vector<double>());
        const std::vector<double> adfMasks(
            bHasFlagMasks ? poFlagMasks->ReadAsDoubleArray()
                          : std::vector<double>());

        if (bHasFlagValues &&
            adfValues.size() != static_cast<size_t>(aosFlagMeanings.size()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Number of values in flag_values attribute is different "
                     "from the one in flag_meanings");
            return false;
        }

        if (bHasFlagMasks &&
            adfMasks.size() != static_cast<size_t>(aosFlagMeanings.size()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Number of values in flag_masks attribute is different "
                     "from the one in flag_meanings");
            return false;
        }

        for (int i = 0; i < aosUnmaskFlags.size(); ++i)
        {
            const int nIdxFlag = aosFlagMeanings.FindString(aosUnmaskFlags[i]);
            if (nIdxFlag < 0)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Cannot fing flag %s in flag_meanings = '%s' attribute",
                    aosUnmaskFlags[i], pszFlagMeanings);
                return false;
            }

            if (bHasFlagValues && adfValues[nIdxFlag] < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid value in flag_values[%d] = %f", nIdxFlag,
                         adfValues[nIdxFlag]);
                return false;
            }

            if (bHasFlagMasks && adfMasks[nIdxFlag] < 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid value in flag_masks[%d] = %f", nIdxFlag,
                         adfMasks[nIdxFlag]);
                return false;
            }

            if (bHasFlagValues)
            {
                m_anValidFlagValues.push_back(
                    static_cast<uint32_t>(adfValues[nIdxFlag]));
            }

            if (bHasFlagMasks)
            {
                m_anValidFlagMasks.push_back(
                    static_cast<uint32_t>(adfMasks[nIdxFlag]));
            }
        }
    }

    return true;
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool GDALMDArrayMask::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                            const GInt64 *arrayStep,
                            const GPtrDiff_t *bufferStride,
                            const GDALExtendedDataType &bufferDataType,
                            void *pDstBuffer) const
{
    if (bufferDataType.GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s: only reading to a numeric data type is supported",
                 __func__);
        return false;
    }
    size_t nElts = 1;
    const size_t nDims = GetDimensionCount();
    std::vector<GPtrDiff_t> tmpBufferStrideVector(nDims);
    for (size_t i = 0; i < nDims; i++)
        nElts *= count[i];
    if (nDims > 0)
    {
        tmpBufferStrideVector.back() = 1;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            tmpBufferStrideVector[i] =
                tmpBufferStrideVector[i + 1] * count[i + 1];
        }
    }

    /* Optimized case: if we are an integer data type and that there is no */
    /* attribute that can be used to set mask = 0, then fill the mask buffer */
    /* directly */
    if (!m_bHasMissingValue && !m_bHasFillValue && !m_bHasValidMin &&
        !m_bHasValidMax && m_anValidFlagValues.empty() &&
        m_anValidFlagMasks.empty() &&
        m_poParent->GetRawNoDataValue() == nullptr &&
        GDALDataTypeIsInteger(m_poParent->GetDataType().GetNumericDataType()))
    {
        const bool bBufferDataTypeIsByte = bufferDataType == m_dt;
        if (bBufferDataTypeIsByte)  // Byte case
        {
            bool bContiguous = true;
            for (size_t i = 0; i < nDims; i++)
            {
                if (bufferStride[i] != tmpBufferStrideVector[i])
                {
                    bContiguous = false;
                    break;
                }
            }
            if (bContiguous)
            {
                // CPLDebug("GDAL", "GetMask(): contiguous case");
                memset(pDstBuffer, 1, nElts);
                return true;
            }
        }

        struct Stack
        {
            size_t nIters = 0;
            GByte *dst_ptr = nullptr;
            GPtrDiff_t dst_inc_offset = 0;
        };

        std::vector<Stack> stack(std::max(static_cast<size_t>(1), nDims));
        const size_t nBufferDTSize = bufferDataType.GetSize();
        for (size_t i = 0; i < nDims; i++)
        {
            stack[i].dst_inc_offset =
                static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
        }
        stack[0].dst_ptr = static_cast<GByte *>(pDstBuffer);

        size_t dimIdx = 0;
        const size_t nDimsMinus1 = nDims > 0 ? nDims - 1 : 0;
        GByte abyOne[16];  // 16 is sizeof GDT_CFloat64
        CPLAssert(nBufferDTSize <= 16);
        const GByte flag = 1;
        GDALCopyWords64(&flag, GDT_UInt8, 0, abyOne,
                        bufferDataType.GetNumericDataType(), 0, 1);

    lbl_next_depth:
        if (dimIdx == nDimsMinus1)
        {
            auto nIters = nDims > 0 ? count[dimIdx] : 1;
            GByte *dst_ptr = stack[dimIdx].dst_ptr;

            while (true)
            {
                // cppcheck-suppress knownConditionTrueFalse
                if (bBufferDataTypeIsByte)
                {
                    *dst_ptr = flag;
                }
                else
                {
                    memcpy(dst_ptr, abyOne, nBufferDTSize);
                }

                if ((--nIters) == 0)
                    break;
                dst_ptr += stack[dimIdx].dst_inc_offset;
            }
        }
        else
        {
            stack[dimIdx].nIters = count[dimIdx];
            while (true)
            {
                dimIdx++;
                stack[dimIdx].dst_ptr = stack[dimIdx - 1].dst_ptr;
                goto lbl_next_depth;
            lbl_return_to_caller:
                dimIdx--;
                if ((--stack[dimIdx].nIters) == 0)
                    break;
                stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
            }
        }
        if (dimIdx > 0)
            goto lbl_return_to_caller;

        return true;
    }

    const auto oTmpBufferDT =
        GDALDataTypeIsComplex(m_poParent->GetDataType().GetNumericDataType())
            ? GDALExtendedDataType::Create(GDT_Float64)
            : m_poParent->GetDataType();
    const size_t nTmpBufferDTSize = oTmpBufferDT.GetSize();
    void *pTempBuffer = VSI_MALLOC2_VERBOSE(nTmpBufferDTSize, nElts);
    if (!pTempBuffer)
        return false;
    if (!m_poParent->Read(arrayStartIdx, count, arrayStep,
                          tmpBufferStrideVector.data(), oTmpBufferDT,
                          pTempBuffer))
    {
        VSIFree(pTempBuffer);
        return false;
    }

    switch (oTmpBufferDT.GetNumericDataType())
    {
        case GDT_UInt8:
            ReadInternal<GByte>(count, bufferStride, bufferDataType, pDstBuffer,
                                pTempBuffer, oTmpBufferDT,
                                tmpBufferStrideVector);
            break;

        case GDT_Int8:
            ReadInternal<GInt8>(count, bufferStride, bufferDataType, pDstBuffer,
                                pTempBuffer, oTmpBufferDT,
                                tmpBufferStrideVector);
            break;

        case GDT_UInt16:
            ReadInternal<GUInt16>(count, bufferStride, bufferDataType,
                                  pDstBuffer, pTempBuffer, oTmpBufferDT,
                                  tmpBufferStrideVector);
            break;

        case GDT_Int16:
            ReadInternal<GInt16>(count, bufferStride, bufferDataType,
                                 pDstBuffer, pTempBuffer, oTmpBufferDT,
                                 tmpBufferStrideVector);
            break;

        case GDT_UInt32:
            ReadInternal<GUInt32>(count, bufferStride, bufferDataType,
                                  pDstBuffer, pTempBuffer, oTmpBufferDT,
                                  tmpBufferStrideVector);
            break;

        case GDT_Int32:
            ReadInternal<GInt32>(count, bufferStride, bufferDataType,
                                 pDstBuffer, pTempBuffer, oTmpBufferDT,
                                 tmpBufferStrideVector);
            break;

        case GDT_UInt64:
            ReadInternal<std::uint64_t>(count, bufferStride, bufferDataType,
                                        pDstBuffer, pTempBuffer, oTmpBufferDT,
                                        tmpBufferStrideVector);
            break;

        case GDT_Int64:
            ReadInternal<std::int64_t>(count, bufferStride, bufferDataType,
                                       pDstBuffer, pTempBuffer, oTmpBufferDT,
                                       tmpBufferStrideVector);
            break;

        case GDT_Float16:
            ReadInternal<GFloat16>(count, bufferStride, bufferDataType,
                                   pDstBuffer, pTempBuffer, oTmpBufferDT,
                                   tmpBufferStrideVector);
            break;

        case GDT_Float32:
            ReadInternal<float>(count, bufferStride, bufferDataType, pDstBuffer,
                                pTempBuffer, oTmpBufferDT,
                                tmpBufferStrideVector);
            break;

        case GDT_Float64:
            ReadInternal<double>(count, bufferStride, bufferDataType,
                                 pDstBuffer, pTempBuffer, oTmpBufferDT,
                                 tmpBufferStrideVector);
            break;
        case GDT_Unknown:
        case GDT_CInt16:
        case GDT_CInt32:
        case GDT_CFloat16:
        case GDT_CFloat32:
        case GDT_CFloat64:
        case GDT_TypeCount:
            CPLAssert(false);
            break;
    }

    VSIFree(pTempBuffer);

    return true;
}

/************************************************************************/
/*                            IsValidForDT()                            */
/************************************************************************/

template <typename Type> static bool IsValidForDT(double dfVal)
{
    if (std::isnan(dfVal))
        return false;
    if (dfVal < static_cast<double>(cpl::NumericLimits<Type>::lowest()))
        return false;
    if (dfVal > static_cast<double>(cpl::NumericLimits<Type>::max()))
        return false;
    return static_cast<double>(static_cast<Type>(dfVal)) == dfVal;
}

template <> bool IsValidForDT<double>(double)
{
    return true;
}

/************************************************************************/
/*                               IsNan()                                */
/************************************************************************/

template <typename Type> inline bool IsNan(Type)
{
    return false;
}

template <> bool IsNan<double>(double val)
{
    return std::isnan(val);
}

template <> bool IsNan<float>(float val)
{
    return std::isnan(val);
}

/************************************************************************/
/*                            ReadInternal()                            */
/************************************************************************/

template <typename Type>
void GDALMDArrayMask::ReadInternal(
    const size_t *count, const GPtrDiff_t *bufferStride,
    const GDALExtendedDataType &bufferDataType, void *pDstBuffer,
    const void *pTempBuffer, const GDALExtendedDataType &oTmpBufferDT,
    const std::vector<GPtrDiff_t> &tmpBufferStrideVector) const
{
    const size_t nDims = GetDimensionCount();

    const auto castValue = [](bool &bHasVal, double dfVal) -> Type
    {
        if (bHasVal)
        {
            if (IsValidForDT<Type>(dfVal))
            {
                return static_cast<Type>(dfVal);
            }
            else
            {
                bHasVal = false;
            }
        }
        return 0;
    };

    const void *pSrcRawNoDataValue = m_poParent->GetRawNoDataValue();
    bool bHasNodataValue = pSrcRawNoDataValue != nullptr;
    const Type nNoDataValue =
        castValue(bHasNodataValue, m_poParent->GetNoDataValueAsDouble());
    bool bHasMissingValue = m_bHasMissingValue;
    const Type nMissingValue = castValue(bHasMissingValue, m_dfMissingValue);
    bool bHasFillValue = m_bHasFillValue;
    const Type nFillValue = castValue(bHasFillValue, m_dfFillValue);
    bool bHasValidMin = m_bHasValidMin;
    const Type nValidMin = castValue(bHasValidMin, m_dfValidMin);
    bool bHasValidMax = m_bHasValidMax;
    const Type nValidMax = castValue(bHasValidMax, m_dfValidMax);
    const bool bHasValidFlags =
        !m_anValidFlagValues.empty() || !m_anValidFlagMasks.empty();

    const auto IsValidFlag = [this](Type v)
    {
        if (!m_anValidFlagValues.empty() && !m_anValidFlagMasks.empty())
        {
            for (size_t i = 0; i < m_anValidFlagValues.size(); ++i)
            {
                if ((static_cast<uint32_t>(v) & m_anValidFlagMasks[i]) ==
                    m_anValidFlagValues[i])
                {
                    return true;
                }
            }
        }
        else if (!m_anValidFlagValues.empty())
        {
            for (size_t i = 0; i < m_anValidFlagValues.size(); ++i)
            {
                if (static_cast<uint32_t>(v) == m_anValidFlagValues[i])
                {
                    return true;
                }
            }
        }
        else /* if( !m_anValidFlagMasks.empty() ) */
        {
            for (size_t i = 0; i < m_anValidFlagMasks.size(); ++i)
            {
                if ((static_cast<uint32_t>(v) & m_anValidFlagMasks[i]) != 0)
                {
                    return true;
                }
            }
        }
        return false;
    };

#define GET_MASK_FOR_SAMPLE(v)                                                 \
    static_cast<GByte>(!IsNan(v) && !(bHasNodataValue && v == nNoDataValue) && \
                       !(bHasMissingValue && v == nMissingValue) &&            \
                       !(bHasFillValue && v == nFillValue) &&                  \
                       !(bHasValidMin && v < nValidMin) &&                     \
                       !(bHasValidMax && v > nValidMax) &&                     \
                       (!bHasValidFlags || IsValidFlag(v)));

    const bool bBufferDataTypeIsByte = bufferDataType == m_dt;
    /* Optimized case: Byte output and output buffer is contiguous */
    if (bBufferDataTypeIsByte)
    {
        bool bContiguous = true;
        for (size_t i = 0; i < nDims; i++)
        {
            if (bufferStride[i] != tmpBufferStrideVector[i])
            {
                bContiguous = false;
                break;
            }
        }
        if (bContiguous)
        {
            size_t nElts = 1;
            for (size_t i = 0; i < nDims; i++)
                nElts *= count[i];

            for (size_t i = 0; i < nElts; i++)
            {
                const Type *pSrc = static_cast<const Type *>(pTempBuffer) + i;
                static_cast<GByte *>(pDstBuffer)[i] =
                    GET_MASK_FOR_SAMPLE(*pSrc);
            }
            return;
        }
    }

    const size_t nTmpBufferDTSize = oTmpBufferDT.GetSize();

    struct Stack
    {
        size_t nIters = 0;
        const GByte *src_ptr = nullptr;
        GByte *dst_ptr = nullptr;
        GPtrDiff_t src_inc_offset = 0;
        GPtrDiff_t dst_inc_offset = 0;
    };

    std::vector<Stack> stack(std::max(static_cast<size_t>(1), nDims));
    const size_t nBufferDTSize = bufferDataType.GetSize();
    for (size_t i = 0; i < nDims; i++)
    {
        stack[i].src_inc_offset = static_cast<GPtrDiff_t>(
            tmpBufferStrideVector[i] * nTmpBufferDTSize);
        stack[i].dst_inc_offset =
            static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
    }
    stack[0].src_ptr = static_cast<const GByte *>(pTempBuffer);
    stack[0].dst_ptr = static_cast<GByte *>(pDstBuffer);

    size_t dimIdx = 0;
    const size_t nDimsMinus1 = nDims > 0 ? nDims - 1 : 0;
    GByte abyZeroOrOne[2][16];  // 16 is sizeof GDT_CFloat64
    CPLAssert(nBufferDTSize <= 16);
    for (GByte flag = 0; flag <= 1; flag++)
    {
        GDALCopyWords64(&flag, m_dt.GetNumericDataType(), 0, abyZeroOrOne[flag],
                        bufferDataType.GetNumericDataType(), 0, 1);
    }

lbl_next_depth:
    if (dimIdx == nDimsMinus1)
    {
        auto nIters = nDims > 0 ? count[dimIdx] : 1;
        const GByte *src_ptr = stack[dimIdx].src_ptr;
        GByte *dst_ptr = stack[dimIdx].dst_ptr;

        while (true)
        {
            const Type *pSrc = reinterpret_cast<const Type *>(src_ptr);
            const GByte flag = GET_MASK_FOR_SAMPLE(*pSrc);

            if (bBufferDataTypeIsByte)
            {
                *dst_ptr = flag;
            }
            else
            {
                memcpy(dst_ptr, abyZeroOrOne[flag], nBufferDTSize);
            }

            if ((--nIters) == 0)
                break;
            src_ptr += stack[dimIdx].src_inc_offset;
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
}

//! @endcond

/************************************************************************/
/*                              GetMask()                               */
/************************************************************************/

/** Return an array that is a mask for the current array

 This array will be of type Byte, with values set to 0 to indicate invalid
 pixels of the current array, and values set to 1 to indicate valid pixels.

 The generic implementation honours the NoDataValue, as well as various
 netCDF CF attributes: missing_value, _FillValue, valid_min, valid_max
 and valid_range.

 Starting with GDAL 3.8, option UNMASK_FLAGS=flag_meaning_1[,flag_meaning_2,...]
 can be used to specify strings of the "flag_meanings" attribute
 (cf https://cfconventions.org/cf-conventions/cf-conventions.html#flags)
 for which pixels matching any of those flags will be set at 1 in the mask array,
 and pixels matching none of those flags will be set at 0.
 For example, let's consider the following netCDF variable defined with:
 \verbatim
 l2p_flags:valid_min = 0s ;
 l2p_flags:valid_max = 256s ;
 l2p_flags:flag_meanings = "microwave land ice lake river reserved_for_future_use unused_currently unused_currently unused_currently" ;
 l2p_flags:flag_masks = 1s, 2s, 4s, 8s, 16s, 32s, 64s, 128s, 256s ;
 \endverbatim

 GetMask(["UNMASK_FLAGS=microwave,land"]) will return an array such that:
 - for pixel values *outside* valid_range [0,256], the mask value will be 0.
 - for a pixel value with bit 0 or bit 1 at 1 within [0,256], the mask value
   will be 1.
 - for a pixel value with bit 0 and bit 1 at 0 within [0,256], the mask value
   will be 0.

 This is the same as the C function GDALMDArrayGetMask().

 @param papszOptions NULL-terminated list of options, or NULL.

 @return a new array, that holds a reference to the original one, and thus is
 a view of it (not a copy), or nullptr in case of error.
*/
std::shared_ptr<GDALMDArray>
GDALMDArray::GetMask(CSLConstList papszOptions) const
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
                 "GetMask() only supports numeric data type");
        return nullptr;
    }
    return GDALMDArrayMask::Create(self, papszOptions);
}
