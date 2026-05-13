/******************************************************************************
 *
 * Name:     gdalmultidim_array.cpp
 * Project:  GDAL Core
 * Purpose:  Implementation of core GDALMDArray functionality
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_float.h"
#include "gdal_multidim.h"
#include "gdalmultidim_array_unscaled.h"
#include "gdal_pam.h"
#include "memmultidim.h"

#include <algorithm>
#include <cmath>
#include <ctype.h>  // isalnum
#include <limits>

#if defined(__clang__) || defined(_MSC_VER)
#define COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT
#endif

/************************************************************************/
/*                              SetUnit()                               */
/************************************************************************/

/** Set the variable unit.
 *
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 *
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 *
 * This is the same as the C function GDALMDArraySetUnit()
 *
 * @note Driver implementation: optionally implemented.
 *
 * @param osUnit unit name.
 * @return true in case of success.
 */
bool GDALMDArray::SetUnit(CPL_UNUSED const std::string &osUnit)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetUnit() not implemented");
    return false;
}

/************************************************************************/
/*                              GetUnit()                               */
/************************************************************************/

/** Return the array unit.
 *
 * Values should conform as much as possible with those allowed by
 * the NetCDF CF conventions:
 * http://cfconventions.org/Data/cf-conventions/cf-conventions-1.7/cf-conventions.html#units
 * but others might be returned.
 *
 * Few examples are "meter", "degrees", "second", ...
 * Empty value means unknown.
 *
 * This is the same as the C function GDALMDArrayGetUnit()
 */
const std::string &GDALMDArray::GetUnit() const
{
    static const std::string emptyString;
    return emptyString;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

/** Assign a spatial reference system object to the array.
 *
 * This is the same as the C function GDALMDArraySetSpatialRef().
 */
bool GDALMDArray::SetSpatialRef(CPL_UNUSED const OGRSpatialReference *poSRS)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetSpatialRef() not implemented");
    return false;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

/** Return the spatial reference system object associated with the array.
 *
 * This is the same as the C function GDALMDArrayGetSpatialRef().
 */
std::shared_ptr<OGRSpatialReference> GDALMDArray::GetSpatialRef() const
{
    return nullptr;
}

/************************************************************************/
/*                         GetRawNoDataValue()                          */
/************************************************************************/

/** Return the nodata value as a "raw" value.
 *
 * The value returned might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr will be returned whose size in
 * bytes is GetDataType().GetSize().
 *
 * The returned value should not be modified or freed. It is valid until
 * the array is destroyed, or the next call to GetRawNoDataValue() or
 * SetRawNoDataValue(), or any similar methods.
 *
 * @note Driver implementation: this method shall be implemented if nodata
 * is supported.
 *
 * This is the same as the C function GDALMDArrayGetRawNoDataValue().
 *
 * @return nullptr or a pointer to GetDataType().GetSize() bytes.
 */
const void *GDALMDArray::GetRawNoDataValue() const
{
    return nullptr;
}

/************************************************************************/
/*                       GetNoDataValueAsDouble()                       */
/************************************************************************/

/** Return the nodata value as a double.
 *
 * This is the same as the C function GDALMDArrayGetNoDataValueAsDouble().
 *
 * @param pbHasNoData Pointer to a output boolean that will be set to true if
 * a nodata value exists and can be converted to double. Might be nullptr.
 *
 * @return the nodata value as a double. A 0.0 value might also indicate the
 * absence of a nodata value or an error in the conversion (*pbHasNoData will be
 * set to false then).
 */
double GDALMDArray::GetNoDataValueAsDouble(bool *pbHasNoData) const
{
    const void *pNoData = GetRawNoDataValue();
    double dfNoData = 0.0;
    const auto &eDT = GetDataType();
    const bool ok = pNoData != nullptr && eDT.GetClass() == GEDTC_NUMERIC;
    if (ok)
    {
        GDALCopyWords64(pNoData, eDT.GetNumericDataType(), 0, &dfNoData,
                        GDT_Float64, 0, 1);
    }
    if (pbHasNoData)
        *pbHasNoData = ok;
    return dfNoData;
}

/************************************************************************/
/*                       GetNoDataValueAsInt64()                        */
/************************************************************************/

/** Return the nodata value as a Int64.
 *
 * @param pbHasNoData Pointer to a output boolean that will be set to true if
 * a nodata value exists and can be converted to Int64. Might be nullptr.
 *
 * This is the same as the C function GDALMDArrayGetNoDataValueAsInt64().
 *
 * @return the nodata value as a Int64
 *
 * @since GDAL 3.5
 */
int64_t GDALMDArray::GetNoDataValueAsInt64(bool *pbHasNoData) const
{
    const void *pNoData = GetRawNoDataValue();
    int64_t nNoData = GDAL_PAM_DEFAULT_NODATA_VALUE_INT64;
    const auto &eDT = GetDataType();
    const bool ok = pNoData != nullptr && eDT.GetClass() == GEDTC_NUMERIC;
    if (ok)
    {
        GDALCopyWords64(pNoData, eDT.GetNumericDataType(), 0, &nNoData,
                        GDT_Int64, 0, 1);
    }
    if (pbHasNoData)
        *pbHasNoData = ok;
    return nNoData;
}

/************************************************************************/
/*                       GetNoDataValueAsUInt64()                       */
/************************************************************************/

/** Return the nodata value as a UInt64.
 *
 * This is the same as the C function GDALMDArrayGetNoDataValueAsUInt64().

 * @param pbHasNoData Pointer to a output boolean that will be set to true if
 * a nodata value exists and can be converted to UInt64. Might be nullptr.
 *
 * @return the nodata value as a UInt64
 *
 * @since GDAL 3.5
 */
uint64_t GDALMDArray::GetNoDataValueAsUInt64(bool *pbHasNoData) const
{
    const void *pNoData = GetRawNoDataValue();
    uint64_t nNoData = GDAL_PAM_DEFAULT_NODATA_VALUE_UINT64;
    const auto &eDT = GetDataType();
    const bool ok = pNoData != nullptr && eDT.GetClass() == GEDTC_NUMERIC;
    if (ok)
    {
        GDALCopyWords64(pNoData, eDT.GetNumericDataType(), 0, &nNoData,
                        GDT_UInt64, 0, 1);
    }
    if (pbHasNoData)
        *pbHasNoData = ok;
    return nNoData;
}

/************************************************************************/
/*                         SetRawNoDataValue()                          */
/************************************************************************/

/** Set the nodata value as a "raw" value.
 *
 * The value passed might be nullptr in case of no nodata value. When
 * a nodata value is registered, a non-nullptr whose size in
 * bytes is GetDataType().GetSize() must be passed.
 *
 * This is the same as the C function GDALMDArraySetRawNoDataValue().
 *
 * @note Driver implementation: this method shall be implemented if setting
 nodata
 * is supported.

 * @return true in case of success.
 */
bool GDALMDArray::SetRawNoDataValue(CPL_UNUSED const void *pRawNoData)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "SetRawNoDataValue() not implemented");
    return false;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

/** Set the nodata value as a double.
 *
 * If the natural data type of the attribute/array is not double, type
 * conversion will occur to the type returned by GetDataType().
 *
 * This is the same as the C function GDALMDArraySetNoDataValueAsDouble().
 *
 * @return true in case of success.
 */
bool GDALMDArray::SetNoDataValue(double dfNoData)
{
    void *pRawNoData = CPLMalloc(GetDataType().GetSize());
    bool bRet = false;
    if (GDALExtendedDataType::CopyValue(
            &dfNoData, GDALExtendedDataType::Create(GDT_Float64), pRawNoData,
            GetDataType()))
    {
        bRet = SetRawNoDataValue(pRawNoData);
    }
    CPLFree(pRawNoData);
    return bRet;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

/** Set the nodata value as a Int64.
 *
 * If the natural data type of the attribute/array is not Int64, type conversion
 * will occur to the type returned by GetDataType().
 *
 * This is the same as the C function GDALMDArraySetNoDataValueAsInt64().
 *
 * @return true in case of success.
 *
 * @since GDAL 3.5
 */
bool GDALMDArray::SetNoDataValue(int64_t nNoData)
{
    void *pRawNoData = CPLMalloc(GetDataType().GetSize());
    bool bRet = false;
    if (GDALExtendedDataType::CopyValue(&nNoData,
                                        GDALExtendedDataType::Create(GDT_Int64),
                                        pRawNoData, GetDataType()))
    {
        bRet = SetRawNoDataValue(pRawNoData);
    }
    CPLFree(pRawNoData);
    return bRet;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

/** Set the nodata value as a Int64.
 *
 * If the natural data type of the attribute/array is not Int64, type conversion
 * will occur to the type returned by GetDataType().
 *
 * This is the same as the C function GDALMDArraySetNoDataValueAsUInt64().
 *
 * @return true in case of success.
 *
 * @since GDAL 3.5
 */
bool GDALMDArray::SetNoDataValue(uint64_t nNoData)
{
    void *pRawNoData = CPLMalloc(GetDataType().GetSize());
    bool bRet = false;
    if (GDALExtendedDataType::CopyValue(
            &nNoData, GDALExtendedDataType::Create(GDT_UInt64), pRawNoData,
            GetDataType()))
    {
        bRet = SetRawNoDataValue(pRawNoData);
    }
    CPLFree(pRawNoData);
    return bRet;
}

/************************************************************************/
/*                               Resize()                               */
/************************************************************************/

/** Resize an array to new dimensions.
 *
 * Not all drivers may allow this operation, and with restrictions (e.g.
 * for netCDF, this is limited to growing of "unlimited" dimensions)
 *
 * Resizing a dimension used in other arrays will cause those other arrays
 * to be resized.
 *
 * This is the same as the C function GDALMDArrayResize().
 *
 * @param anNewDimSizes Array of GetDimensionCount() values containing the
 *                      new size of each indexing dimension.
 * @param papszOptions Options. (Driver specific)
 * @return true in case of success.
 * @since GDAL 3.7
 */
bool GDALMDArray::Resize(CPL_UNUSED const std::vector<GUInt64> &anNewDimSizes,
                         CPL_UNUSED CSLConstList papszOptions)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Resize() is not supported for this array");
    return false;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

/** Set the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArraySetScale() /
 * GDALMDArraySetScaleEx().
 *
 * @note Driver implementation: this method shall be implemented if setting
 * scale is supported.
 *
 * @param dfScale scale
 * @param eStorageType Data type to which create the potential attribute that
 * will store the scale. Added in GDAL 3.3 If let to its GDT_Unknown value, the
 * implementation will decide automatically the data type. Note that changing
 * the data type after initial setting might not be supported.
 * @return true in case of success.
 */
bool GDALMDArray::SetScale(CPL_UNUSED double dfScale,
                           CPL_UNUSED GDALDataType eStorageType)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetScale() not implemented");
    return false;
}

/************************************************************************/
/*                              SetOffset)                              */
/************************************************************************/

/** Set the offset value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArraySetOffset() /
 * GDALMDArraySetOffsetEx().
 *
 * @note Driver implementation: this method shall be implemented if setting
 * offset is supported.
 *
 * @param dfOffset Offset
 * @param eStorageType Data type to which create the potential attribute that
 * will store the offset. Added in GDAL 3.3 If let to its GDT_Unknown value, the
 * implementation will decide automatically the data type. Note that changing
 * the data type after initial setting might not be supported.
 * @return true in case of success.
 */
bool GDALMDArray::SetOffset(CPL_UNUSED double dfOffset,
                            CPL_UNUSED GDALDataType eStorageType)
{
    CPLError(CE_Failure, CPLE_NotSupported, "SetOffset() not implemented");
    return false;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

/** Get the scale value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArrayGetScale().
 *
 * @note Driver implementation: this method shall be implemented if getting
 * scale is supported.
 *
 * @param pbHasScale Pointer to a output boolean that will be set to true if
 * a scale value exists. Might be nullptr.
 * @param peStorageType Pointer to a output GDALDataType that will be set to
 * the storage type of the scale value, when known/relevant. Otherwise will be
 * set to GDT_Unknown. Might be nullptr. Since GDAL 3.3
 *
 * @return the scale value. A 1.0 value might also indicate the
 * absence of a scale value.
 */
double GDALMDArray::GetScale(CPL_UNUSED bool *pbHasScale,
                             CPL_UNUSED GDALDataType *peStorageType) const
{
    if (pbHasScale)
        *pbHasScale = false;
    return 1.0;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

/** Get the offset value to apply to raw values.
 *
 * unscaled_value = raw_value * GetScale() + GetOffset()
 *
 * This is the same as the C function GDALMDArrayGetOffset().
 *
 * @note Driver implementation: this method shall be implemented if getting
 * offset is supported.
 *
 * @param pbHasOffset Pointer to a output boolean that will be set to true if
 * a offset value exists. Might be nullptr.
 * @param peStorageType Pointer to a output GDALDataType that will be set to
 * the storage type of the offset value, when known/relevant. Otherwise will be
 * set to GDT_Unknown. Might be nullptr. Since GDAL 3.3
 *
 * @return the offset value. A 0.0 value might also indicate the
 * absence of a offset value.
 */
double GDALMDArray::GetOffset(CPL_UNUSED bool *pbHasOffset,
                              CPL_UNUSED GDALDataType *peStorageType) const
{
    if (pbHasOffset)
        *pbHasOffset = false;
    return 0.0;
}

/************************************************************************/
/*                            GDALMDArray()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALMDArray::GDALMDArray(CPL_UNUSED const std::string &osParentName,
                         CPL_UNUSED const std::string &osName,
                         const std::string &osContext)
    :
#if !defined(COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT)
      GDALAbstractMDArray(osParentName, osName),
#endif
      m_osContext(osContext)
{
}

//! @endcond

/************************************************************************/
/*                          GetTotalCopyCost()                          */
/************************************************************************/

/** Return a total "cost" to copy the array.
 *
 * Used as a parameter for CopyFrom()
 */
GUInt64 GDALMDArray::GetTotalCopyCost() const
{
    return COPY_COST + GetAttributes().size() * GDALAttribute::COPY_COST +
           GetTotalElementsCount() * GetDataType().GetSize();
}

/************************************************************************/
/*                      CopyFromAllExceptValues()                       */
/************************************************************************/

//! @cond Doxygen_Suppress

bool GDALMDArray::CopyFromAllExceptValues(const GDALMDArray *poSrcArray,
                                          bool bStrict, GUInt64 &nCurCost,
                                          const GUInt64 nTotalCost,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)
{
    // Nodata setting must be one of the first things done for TileDB
    const void *pNoData = poSrcArray->GetRawNoDataValue();
    if (pNoData && poSrcArray->GetDataType() == GetDataType())
    {
        SetRawNoDataValue(pNoData);
    }

    const bool bThisIsUnscaledArray =
        dynamic_cast<GDALMDArrayUnscaled *>(this) != nullptr;
    auto attrs = poSrcArray->GetAttributes();
    for (const auto &attr : attrs)
    {
        const auto &osAttrName = attr->GetName();
        if (bThisIsUnscaledArray)
        {
            if (osAttrName == "missing_value" || osAttrName == "_FillValue" ||
                osAttrName == "valid_min" || osAttrName == "valid_max" ||
                osAttrName == "valid_range")
            {
                continue;
            }
        }

        auto dstAttr = CreateAttribute(osAttrName, attr->GetDimensionsSize(),
                                       attr->GetDataType());
        if (!dstAttr)
        {
            if (bStrict)
                return false;
            continue;
        }
        auto raw = attr->ReadAsRaw();
        if (!dstAttr->Write(raw.data(), raw.size()) && bStrict)
            return false;
    }
    if (!attrs.empty())
    {
        nCurCost += attrs.size() * GDALAttribute::COPY_COST;
        if (pfnProgress &&
            !pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData))
            return false;
    }

    auto srcSRS = poSrcArray->GetSpatialRef();
    if (srcSRS)
    {
        SetSpatialRef(srcSRS.get());
    }

    const std::string &osUnit(poSrcArray->GetUnit());
    if (!osUnit.empty())
    {
        SetUnit(osUnit);
    }

    bool bGotValue = false;
    GDALDataType eOffsetStorageType = GDT_Unknown;
    const double dfOffset =
        poSrcArray->GetOffset(&bGotValue, &eOffsetStorageType);
    if (bGotValue)
    {
        SetOffset(dfOffset, eOffsetStorageType);
    }

    bGotValue = false;
    GDALDataType eScaleStorageType = GDT_Unknown;
    const double dfScale = poSrcArray->GetScale(&bGotValue, &eScaleStorageType);
    if (bGotValue)
    {
        SetScale(dfScale, eScaleStorageType);
    }

    return true;
}

//! @endcond

/************************************************************************/
/*                              CopyFrom()                              */
/************************************************************************/

/** Copy the content of an array into a new (generally empty) array.
 *
 * @param poSrcDS    Source dataset. Might be nullptr (but for correct behavior
 *                   of some output drivers this is not recommended)
 * @param poSrcArray Source array. Should NOT be nullptr.
 * @param bStrict Whether to enable strict mode. In strict mode, any error will
 *                stop the copy. In relaxed mode, the copy will be attempted to
 *                be pursued.
 * @param nCurCost  Should be provided as a variable initially set to 0.
 * @param nTotalCost Total cost from GetTotalCopyCost().
 * @param pfnProgress Progress callback, or nullptr.
 * @param pProgressData Progress user data, or nullptr.
 *
 * @return true in case of success (or partial success if bStrict == false).
 */
bool GDALMDArray::CopyFrom(CPL_UNUSED GDALDataset *poSrcDS,
                           const GDALMDArray *poSrcArray, bool bStrict,
                           GUInt64 &nCurCost, const GUInt64 nTotalCost,
                           GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    nCurCost += GDALMDArray::COPY_COST;

    if (!CopyFromAllExceptValues(poSrcArray, bStrict, nCurCost, nTotalCost,
                                 pfnProgress, pProgressData))
    {
        return false;
    }

    const auto &dims = poSrcArray->GetDimensions();
    const auto nDTSize = poSrcArray->GetDataType().GetSize();
    if (dims.empty())
    {
        std::vector<GByte> abyTmp(nDTSize);
        if (!(poSrcArray->Read(nullptr, nullptr, nullptr, nullptr,
                               GetDataType(), &abyTmp[0]) &&
              Write(nullptr, nullptr, nullptr, nullptr, GetDataType(),
                    &abyTmp[0])) &&
            bStrict)
        {
            return false;
        }
        nCurCost += GetTotalElementsCount() * GetDataType().GetSize();
        if (!pfnProgress(double(nCurCost) / nTotalCost, "", pProgressData))
            return false;
    }
    else
    {
        std::vector<GUInt64> arrayStartIdx(dims.size());
        std::vector<GUInt64> count(dims.size());
        for (size_t i = 0; i < dims.size(); i++)
        {
            count[i] = static_cast<size_t>(dims[i]->GetSize());
        }

        struct CopyFunc
        {
            GDALMDArray *poDstArray = nullptr;
            std::vector<GByte> abyTmp{};
            GDALProgressFunc pfnProgress = nullptr;
            void *pProgressData = nullptr;
            GUInt64 nCurCost = 0;
            GUInt64 nTotalCost = 0;
            GUInt64 nTotalBytesThisArray = 0;
            bool bStop = false;

            static bool f(GDALAbstractMDArray *l_poSrcArray,
                          const GUInt64 *chunkArrayStartIdx,
                          const size_t *chunkCount, GUInt64 iCurChunk,
                          GUInt64 nChunkCount, void *pUserData)
            {
                const auto &dt(l_poSrcArray->GetDataType());
                auto data = static_cast<CopyFunc *>(pUserData);
                auto poDstArray = data->poDstArray;
                if (!l_poSrcArray->Read(chunkArrayStartIdx, chunkCount, nullptr,
                                        nullptr, dt, &data->abyTmp[0]))
                {
                    return false;
                }
                bool bRet =
                    poDstArray->Write(chunkArrayStartIdx, chunkCount, nullptr,
                                      nullptr, dt, &data->abyTmp[0]);
                if (dt.NeedsFreeDynamicMemory())
                {
                    const auto l_nDTSize = dt.GetSize();
                    GByte *ptr = &data->abyTmp[0];
                    const size_t l_nDims(l_poSrcArray->GetDimensionCount());
                    size_t nEltCount = 1;
                    for (size_t i = 0; i < l_nDims; ++i)
                    {
                        nEltCount *= chunkCount[i];
                    }
                    for (size_t i = 0; i < nEltCount; i++)
                    {
                        dt.FreeDynamicMemory(ptr);
                        ptr += l_nDTSize;
                    }
                }
                if (!bRet)
                {
                    return false;
                }

                double dfCurCost =
                    double(data->nCurCost) + double(iCurChunk) / nChunkCount *
                                                 data->nTotalBytesThisArray;
                if (!data->pfnProgress(dfCurCost / data->nTotalCost, "",
                                       data->pProgressData))
                {
                    data->bStop = true;
                    return false;
                }

                return true;
            }
        };

        CopyFunc copyFunc;
        copyFunc.poDstArray = this;
        copyFunc.nCurCost = nCurCost;
        copyFunc.nTotalCost = nTotalCost;
        copyFunc.nTotalBytesThisArray = GetTotalElementsCount() * nDTSize;
        copyFunc.pfnProgress = pfnProgress;
        copyFunc.pProgressData = pProgressData;
        const char *pszSwathSize =
            CPLGetConfigOption("GDAL_SWATH_SIZE", nullptr);
        const size_t nMaxChunkSize =
            pszSwathSize
                ? static_cast<size_t>(
                      std::min(GIntBig(std::numeric_limits<size_t>::max() / 2),
                               CPLAtoGIntBig(pszSwathSize)))
                : static_cast<size_t>(
                      std::min(GIntBig(std::numeric_limits<size_t>::max() / 2),
                               GDALGetCacheMax64() / 4));
        const auto anChunkSizes(GetProcessingChunkSize(nMaxChunkSize));
        size_t nRealChunkSize = nDTSize;
        for (const auto &nChunkSize : anChunkSizes)
        {
            nRealChunkSize *= nChunkSize;
        }
        try
        {
            copyFunc.abyTmp.resize(nRealChunkSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Cannot allocate temporary buffer");
            nCurCost += copyFunc.nTotalBytesThisArray;
            return false;
        }
        if (copyFunc.nTotalBytesThisArray != 0 &&
            !const_cast<GDALMDArray *>(poSrcArray)
                 ->ProcessPerChunk(arrayStartIdx.data(), count.data(),
                                   anChunkSizes.data(), CopyFunc::f,
                                   &copyFunc) &&
            (bStrict || copyFunc.bStop))
        {
            nCurCost += copyFunc.nTotalBytesThisArray;
            return false;
        }
        nCurCost += copyFunc.nTotalBytesThisArray;
    }

    return true;
}

/************************************************************************/
/*                         GetStructuralInfo()                          */
/************************************************************************/

/** Return structural information on the array.
 *
 * This may be the compression, etc..
 *
 * The return value should not be freed and is valid until GDALMDArray is
 * released or this function called again.
 *
 * This is the same as the C function GDALMDArrayGetStructuralInfo().
 */
CSLConstList GDALMDArray::GetStructuralInfo() const
{
    return nullptr;
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

/** Advise driver of upcoming read requests.
 *
 * Some GDAL drivers operate more efficiently if they know in advance what
 * set of upcoming read requests will be made.  The AdviseRead() method allows
 * an application to notify the driver of the region of interest.
 *
 * Many drivers just ignore the AdviseRead() call, but it can dramatically
 * accelerate access via some drivers. One such case is when reading through
 * a DAP dataset with the netCDF driver (a in-memory cache array is then created
 * with the region of interest defined by AdviseRead())
 *
 * This is the same as the C function GDALMDArrayAdviseRead().
 *
 * @param arrayStartIdx Values representing the starting index to read
 *                      in each dimension (in [0, aoDims[i].GetSize()-1] range).
 *                      Array of GetDimensionCount() values.
 *                      Can be nullptr as a synonymous for [0 for i in
 * range(GetDimensionCount() ]
 *
 * @param count         Values representing the number of values to extract in
 *                      each dimension.
 *                      Array of GetDimensionCount() values.
 *                      Can be nullptr as a synonymous for
 *                      [ aoDims[i].GetSize() - arrayStartIdx[i] for i in
 * range(GetDimensionCount() ]
 *
 * @param papszOptions Driver specific options, or nullptr. Consult driver
 * documentation.
 *
 * @return true in case of success (ignoring the advice is a success)
 *
 * @since GDAL 3.2
 */
bool GDALMDArray::AdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                             CSLConstList papszOptions) const
{
    const auto nDimCount = GetDimensionCount();
    if (nDimCount == 0)
        return true;

    std::vector<GUInt64> tmp_arrayStartIdx;
    if (arrayStartIdx == nullptr)
    {
        tmp_arrayStartIdx.resize(nDimCount);
        arrayStartIdx = tmp_arrayStartIdx.data();
    }

    std::vector<size_t> tmp_count;
    if (count == nullptr)
    {
        tmp_count.resize(nDimCount);
        const auto &dims = GetDimensions();
        for (size_t i = 0; i < nDimCount; i++)
        {
            const GUInt64 nSize = dims[i]->GetSize() - arrayStartIdx[i];
#if SIZEOF_VOIDP < 8
            if (nSize != static_cast<size_t>(nSize))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow");
                return false;
            }
#endif
            tmp_count[i] = static_cast<size_t>(nSize);
        }
        count = tmp_count.data();
    }

    std::vector<GInt64> tmp_arrayStep;
    std::vector<GPtrDiff_t> tmp_bufferStride;
    const GInt64 *arrayStep = nullptr;
    const GPtrDiff_t *bufferStride = nullptr;
    if (!CheckReadWriteParams(arrayStartIdx, count, arrayStep, bufferStride,
                              GDALExtendedDataType::Create(GDT_Unknown),
                              nullptr, nullptr, 0, tmp_arrayStep,
                              tmp_bufferStride))
    {
        return false;
    }

    return IAdviseRead(arrayStartIdx, count, papszOptions);
}

/************************************************************************/
/*                            IAdviseRead()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
bool GDALMDArray::IAdviseRead(const GUInt64 *, const size_t *,
                              CSLConstList /* papszOptions*/) const
{
    return true;
}

//! @endcond

/************************************************************************/
/*                            MassageName()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
/*static*/ std::string GDALMDArray::MassageName(const std::string &inputName)
{
    std::string ret;
    for (const char ch : inputName)
    {
        if (!isalnum(static_cast<unsigned char>(ch)))
            ret += '_';
        else
            ret += ch;
    }
    return ret;
}

//! @endcond

/************************************************************************/
/*                         GetCacheRootGroup()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
std::shared_ptr<GDALGroup>
GDALMDArray::GetCacheRootGroup(bool bCanCreate,
                               std::string &osCacheFilenameOut) const
{
    const auto &osFilename = GetFilename();
    if (osFilename.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot cache an array with an empty filename");
        return nullptr;
    }

    osCacheFilenameOut = osFilename + ".gmac";
    if (STARTS_WITH(osFilename.c_str(), "/vsicurl/http"))
    {
        const auto nPosQuestionMark = osFilename.find('?');
        if (nPosQuestionMark != std::string::npos)
        {
            osCacheFilenameOut =
                osFilename.substr(0, nPosQuestionMark)
                    .append(".gmac")
                    .append(osFilename.substr(nPosQuestionMark));
        }
    }
    const char *pszProxy = PamGetProxy(osCacheFilenameOut.c_str());
    if (pszProxy != nullptr)
        osCacheFilenameOut = pszProxy;

    // .gmac sidecars are local-only; skip stat for non-local filesystems.
    if (!bCanCreate && pszProxy == nullptr &&
        !VSIIsLocal(osCacheFilenameOut.c_str()))
    {
        return nullptr;
    }

    std::unique_ptr<GDALDataset> poDS;
    VSIStatBufL sStat;
    if (VSIStatL(osCacheFilenameOut.c_str(), &sStat) == 0)
    {
        poDS.reset(GDALDataset::Open(osCacheFilenameOut.c_str(),
                                     GDAL_OF_MULTIDIM_RASTER | GDAL_OF_UPDATE,
                                     nullptr, nullptr, nullptr));
    }
    if (poDS)
    {
        CPLDebug("GDAL", "Opening cache %s", osCacheFilenameOut.c_str());
        return poDS->GetRootGroup();
    }

    if (bCanCreate)
    {
        const char *pszDrvName = "netCDF";
        GDALDriver *poDrv = GetGDALDriverManager()->GetDriverByName(pszDrvName);
        if (poDrv == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver %s",
                     pszDrvName);
            return nullptr;
        }
        {
            CPLErrorHandlerPusher oHandlerPusher(CPLQuietErrorHandler);
            CPLErrorStateBackuper oErrorStateBackuper;
            poDS.reset(poDrv->CreateMultiDimensional(osCacheFilenameOut.c_str(),
                                                     nullptr, nullptr));
        }
        if (!poDS)
        {
            pszProxy = PamAllocateProxy(osCacheFilenameOut.c_str());
            if (pszProxy)
            {
                osCacheFilenameOut = pszProxy;
                poDS.reset(poDrv->CreateMultiDimensional(
                    osCacheFilenameOut.c_str(), nullptr, nullptr));
            }
        }
        if (poDS)
        {
            CPLDebug("GDAL", "Creating cache %s", osCacheFilenameOut.c_str());
            return poDS->GetRootGroup();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create %s. Set the GDAL_PAM_PROXY_DIR "
                     "configuration option to write the cache in "
                     "another directory",
                     osCacheFilenameOut.c_str());
        }
    }

    return nullptr;
}

//! @endcond

/************************************************************************/
/*                               Cache()                                */
/************************************************************************/

/** Cache the content of the array into an auxiliary filename.
 *
 * The main purpose of this method is to be able to cache views that are
 * expensive to compute, such as transposed arrays.
 *
 * The array will be stored in a file whose name is the one of
 * GetFilename(), with an extra .gmac extension (stands for GDAL
 * Multidimensional Array Cache). The cache is a netCDF dataset.
 *
 * If the .gmac file cannot be written next to the dataset, the
 * GDAL_PAM_PROXY_DIR will be used, if set, to write the cache file into that
 * directory.
 *
 * The GDALMDArray::Read() method will automatically use the cache when it
 * exists. There is no timestamp checks between the source array and the cached
 * array. If the source arrays changes, the cache must be manually deleted.
 *
 * This is the same as the C function GDALMDArrayCache()
 *
 * @note Driver implementation: optionally implemented.
 *
 * @param papszOptions List of options, null terminated, or NULL. Currently
 *                     the only option supported is BLOCKSIZE=bs0,bs1,...,bsN
 *                     to specify the block size of the cached array.
 * @return true in case of success.
 */
bool GDALMDArray::Cache(CSLConstList papszOptions) const
{
    std::string osCacheFilename;
    auto poRG = GetCacheRootGroup(true, osCacheFilename);
    if (!poRG)
        return false;

    const std::string osCachedArrayName(MassageName(GetFullName()));
    if (poRG->OpenMDArray(osCachedArrayName))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "An array with same name %s already exists in %s",
                 osCachedArrayName.c_str(), osCacheFilename.c_str());
        return false;
    }

    CPLStringList aosOptions;
    aosOptions.SetNameValue("COMPRESS", "DEFLATE");
    const auto &aoDims = GetDimensions();
    std::vector<std::shared_ptr<GDALDimension>> aoNewDims;
    if (!aoDims.empty())
    {
        std::string osBlockSize(
            CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", ""));
        if (osBlockSize.empty())
        {
            const auto anBlockSize = GetBlockSize();
            int idxDim = 0;
            for (auto nBlockSize : anBlockSize)
            {
                if (idxDim > 0)
                    osBlockSize += ',';
                if (nBlockSize == 0)
                    nBlockSize = 256;
                nBlockSize = std::min(nBlockSize, aoDims[idxDim]->GetSize());
                osBlockSize +=
                    std::to_string(static_cast<uint64_t>(nBlockSize));
                idxDim++;
            }
        }
        aosOptions.SetNameValue("BLOCKSIZE", osBlockSize.c_str());

        int idxDim = 0;
        for (const auto &poDim : aoDims)
        {
            auto poNewDim = poRG->CreateDimension(
                osCachedArrayName + '_' + std::to_string(idxDim),
                poDim->GetType(), poDim->GetDirection(), poDim->GetSize());
            if (!poNewDim)
                return false;
            aoNewDims.emplace_back(poNewDim);
            idxDim++;
        }
    }

    auto poCachedArray = poRG->CreateMDArray(osCachedArrayName, aoNewDims,
                                             GetDataType(), aosOptions.List());
    if (!poCachedArray)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Cannot create %s in %s",
                 osCachedArrayName.c_str(), osCacheFilename.c_str());
        return false;
    }

    GUInt64 nCost = 0;
    return poCachedArray->CopyFrom(nullptr, this,
                                   false,  // strict
                                   nCost, GetTotalCopyCost(), nullptr, nullptr);
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

bool GDALMDArray::Read(const GUInt64 *arrayStartIdx, const size_t *count,
                       const GInt64 *arrayStep,         // step in elements
                       const GPtrDiff_t *bufferStride,  // stride in elements
                       const GDALExtendedDataType &bufferDataType,
                       void *pDstBuffer, const void *pDstBufferAllocStart,
                       size_t nDstBufferAllocSize) const
{
    if (!m_bHasTriedCachedArray)
    {
        m_bHasTriedCachedArray = true;
        if (IsCacheable())
        {
            const auto &osFilename = GetFilename();
            if (!osFilename.empty() &&
                !EQUAL(CPLGetExtensionSafe(osFilename.c_str()).c_str(), "gmac"))
            {
                std::string osCacheFilename;
                auto poRG = GetCacheRootGroup(false, osCacheFilename);
                if (poRG)
                {
                    const std::string osCachedArrayName(
                        MassageName(GetFullName()));
                    m_poCachedArray = poRG->OpenMDArray(osCachedArrayName);
                    if (m_poCachedArray)
                    {
                        const auto &dims = GetDimensions();
                        const auto &cachedDims =
                            m_poCachedArray->GetDimensions();
                        const size_t nDims = dims.size();
                        bool ok =
                            m_poCachedArray->GetDataType() == GetDataType() &&
                            cachedDims.size() == nDims;
                        for (size_t i = 0; ok && i < nDims; ++i)
                        {
                            ok = dims[i]->GetSize() == cachedDims[i]->GetSize();
                        }
                        if (ok)
                        {
                            CPLDebug("GDAL", "Cached array for %s found in %s",
                                     osCachedArrayName.c_str(),
                                     osCacheFilename.c_str());
                        }
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Cached array %s in %s has incompatible "
                                     "characteristics with current array.",
                                     osCachedArrayName.c_str(),
                                     osCacheFilename.c_str());
                            m_poCachedArray.reset();
                        }
                    }
                }
            }
        }
    }

    const auto array = m_poCachedArray ? m_poCachedArray.get() : this;
    if (!array->GetDataType().CanConvertTo(bufferDataType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array data type is not convertible to buffer data type");
        return false;
    }

    std::vector<GInt64> tmp_arrayStep;
    std::vector<GPtrDiff_t> tmp_bufferStride;
    if (!array->CheckReadWriteParams(arrayStartIdx, count, arrayStep,
                                     bufferStride, bufferDataType, pDstBuffer,
                                     pDstBufferAllocStart, nDstBufferAllocSize,
                                     tmp_arrayStep, tmp_bufferStride))
    {
        return false;
    }

    return array->IRead(arrayStartIdx, count, arrayStep, bufferStride,
                        bufferDataType, pDstBuffer);
}

/************************************************************************/
/*                            GetRootGroup()                            */
/************************************************************************/

/** Return the root group to which this arrays belongs too.
 *
 * Note that arrays may be free standing and some drivers may not implement
 * this method, hence nullptr may be returned.
 *
 * It is used internally by the GetResampled() method to detect if GLT
 * orthorectification is available.
 *
 * @return the root group, or nullptr.
 * @since GDAL 3.8
 */
std::shared_ptr<GDALGroup> GDALMDArray::GetRootGroup() const
{
    return nullptr;
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*           IsStepOneContiguousRowMajorOrderedSameDataType()           */
/************************************************************************/

// Returns true if at all following conditions are met:
// arrayStep[] == 1, bufferDataType == GetDataType() and bufferStride[]
// defines a row-major ordered contiguous buffer.
bool GDALMDArray::IsStepOneContiguousRowMajorOrderedSameDataType(
    const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride,
    const GDALExtendedDataType &bufferDataType) const
{
    if (bufferDataType != GetDataType())
        return false;
    size_t nExpectedStride = 1;
    for (size_t i = GetDimensionCount(); i > 0;)
    {
        --i;
        if (arrayStep[i] != 1 || bufferStride[i] < 0 ||
            static_cast<size_t>(bufferStride[i]) != nExpectedStride)
        {
            return false;
        }
        nExpectedStride *= count[i];
    }
    return true;
}

/************************************************************************/
/*                      ReadUsingContiguousIRead()                      */
/************************************************************************/

// Used for example by the TileDB driver when requesting it with
// arrayStep[] != 1, bufferDataType != GetDataType() or bufferStride[]
// not defining a row-major ordered contiguous buffer.
// Should only be called when at least one of the above conditions are true,
// which can be tested with IsStepOneContiguousRowMajorOrderedSameDataType()
// returning none.
// This method will call IRead() again with arrayStep[] == 1,
// bufferDataType == GetDataType() and bufferStride[] defining a row-major
// ordered contiguous buffer, on a temporary buffer. And it will rearrange the
// content of that temporary buffer onto pDstBuffer.
bool GDALMDArray::ReadUsingContiguousIRead(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride, const GDALExtendedDataType &bufferDataType,
    void *pDstBuffer) const
{
    const size_t nDims(GetDimensionCount());
    std::vector<GUInt64> anTmpStartIdx(nDims);
    std::vector<size_t> anTmpCount(nDims);
    const auto &oType = GetDataType();
    size_t nMemArraySize = oType.GetSize();
    std::vector<GPtrDiff_t> anTmpStride(nDims);
    GPtrDiff_t nStride = 1;
    for (size_t i = nDims; i > 0;)
    {
        --i;
        if (arrayStep[i] > 0)
            anTmpStartIdx[i] = arrayStartIdx[i];
        else
            anTmpStartIdx[i] =
                arrayStartIdx[i] - (count[i] - 1) * (-arrayStep[i]);
        const uint64_t nCount =
            (count[i] - 1) * static_cast<uint64_t>(std::abs(arrayStep[i])) + 1;
        if (nCount > std::numeric_limits<size_t>::max() / nMemArraySize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Read() failed due to too large memory requirement");
            return false;
        }
        anTmpCount[i] = static_cast<size_t>(nCount);
        nMemArraySize *= anTmpCount[i];
        anTmpStride[i] = nStride;
        nStride *= anTmpCount[i];
    }
    std::unique_ptr<void, decltype(&VSIFree)> pTmpBuffer(
        VSI_MALLOC_VERBOSE(nMemArraySize), VSIFree);
    if (!pTmpBuffer)
        return false;
    if (!IRead(anTmpStartIdx.data(), anTmpCount.data(),
               std::vector<GInt64>(nDims, 1).data(),  // steps
               anTmpStride.data(), oType, pTmpBuffer.get()))
    {
        return false;
    }
    std::vector<std::shared_ptr<GDALDimension>> apoTmpDims(nDims);
    for (size_t i = 0; i < nDims; ++i)
    {
        if (arrayStep[i] > 0)
            anTmpStartIdx[i] = 0;
        else
            anTmpStartIdx[i] = anTmpCount[i] - 1;
        apoTmpDims[i] = std::make_shared<GDALDimension>(
            std::string(), std::string(), std::string(), std::string(),
            anTmpCount[i]);
    }
    auto poMEMArray =
        MEMMDArray::Create(std::string(), std::string(), apoTmpDims, oType);
    return poMEMArray->Init(static_cast<GByte *>(pTmpBuffer.get())) &&
           poMEMArray->Read(anTmpStartIdx.data(), count, arrayStep,
                            bufferStride, bufferDataType, pDstBuffer);
}

//! @endcond

/************************************************************************/
/*                         GuessGeoTransform()                          */
/************************************************************************/

/** Returns whether 2 specified dimensions form a geotransform
 *
 * @param nDimX                Index of the X axis (in [0, GetDimensionCount()-1] range).
 * @param nDimY                Index of the Y axis (in [0, GetDimensionCount()-1] range).
 * @param bPixelIsPoint        Whether the geotransform should be returned
 *                             with the pixel-is-point (pixel-center) convention
 *                             (bPixelIsPoint = true), or with the pixel-is-area
 *                             (top left corner convention)
 *                             (bPixelIsPoint = false)
 * @param[out] gt              Computed geotransform
 * @return true if a geotransform could be computed.
 */
bool GDALMDArray::GuessGeoTransform(size_t nDimX, size_t nDimY,
                                    bool bPixelIsPoint,
                                    GDALGeoTransform &gt) const
{
    const auto &dims(GetDimensions());
    auto poVarX = dims[nDimX]->GetIndexingVariable();
    auto poVarY = dims[nDimY]->GetIndexingVariable();
    double dfXStart = 0.0;
    double dfXSpacing = 0.0;
    double dfYStart = 0.0;
    double dfYSpacing = 0.0;
    if (poVarX && poVarX->GetDimensionCount() == 1 &&
        poVarX->GetDimensions()[0]->GetSize() == dims[nDimX]->GetSize() &&
        poVarY && poVarY->GetDimensionCount() == 1 &&
        poVarY->GetDimensions()[0]->GetSize() == dims[nDimY]->GetSize() &&
        poVarX->IsRegularlySpaced(dfXStart, dfXSpacing) &&
        poVarY->IsRegularlySpaced(dfYStart, dfYSpacing))
    {
        gt.xorig = dfXStart - (bPixelIsPoint ? 0 : dfXSpacing / 2);
        gt.xscale = dfXSpacing;
        gt.xrot = 0;
        gt.yorig = dfYStart - (bPixelIsPoint ? 0 : dfYSpacing / 2);
        gt.yrot = 0;
        gt.yscale = dfYSpacing;
        return true;
    }
    return false;
}

/** Returns whether 2 specified dimensions form a geotransform
 *
 * @param nDimX                Index of the X axis (in [0, GetDimensionCount()-1] range).
 * @param nDimY                Index of the Y axis (in [0, GetDimensionCount()-1] range).
 * @param bPixelIsPoint        Whether the geotransform should be returned
 *                             with the pixel-is-point (pixel-center) convention
 *                             (bPixelIsPoint = true), or with the pixel-is-area
 *                             (top left corner convention)
 *                             (bPixelIsPoint = false)
 * @param[out] adfGeoTransform Computed geotransform
 * @return true if a geotransform could be computed.
 */
bool GDALMDArray::GuessGeoTransform(size_t nDimX, size_t nDimY,
                                    bool bPixelIsPoint,
                                    double adfGeoTransform[6]) const
{
    GDALGeoTransform *gt =
        reinterpret_cast<GDALGeoTransform *>(adfGeoTransform);
    return GuessGeoTransform(nDimX, nDimY, bPixelIsPoint, *gt);
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

/**
 * \brief Fetch statistics.
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this array.
 *
 * If bForce is FALSE results will only be returned if it can be done
 * quickly (i.e. without scanning the data).  If bForce is FALSE and
 * results cannot be returned efficiently, the method will return CE_Warning
 * but no warning will have been issued.   This is a non-standard use of
 * the CE_Warning return value to indicate "nothing done".
 *
 * When cached statistics are not available, and bForce is TRUE,
 * ComputeStatistics() is called.
 *
 * Note that file formats using PAM (Persistent Auxiliary Metadata) services
 * will generally cache statistics in the .aux.xml file allowing fast fetch
 * after the first request.
 *
 * Cached statistics can be cleared with GDALDataset::ClearStatistics().
 *
 * This method is the same as the C function GDALMDArrayGetStatistics().
 *
 * @param bApproxOK Currently ignored. In the future, should be set to true
 * if statistics on the whole array are wished, or to false if a subset of it
 * may be used.
 *
 * @param bForce If false statistics will only be returned if it can
 * be done without rescanning the image.
 *
 * @param pdfMin Location into which to load image minimum (may be NULL).
 *
 * @param pdfMax Location into which to load image maximum (may be NULL).-
 *
 * @param pdfMean Location into which to load image mean (may be NULL).
 *
 * @param pdfStdDev Location into which to load image standard deviation
 * (may be NULL).
 *
 * @param pnValidCount Number of samples whose value is different from the
 * nodata value. (may be NULL)
 *
 * @param pfnProgress a function to call to report progress, or NULL.
 *
 * @param pProgressData application data to pass to the progress function.
 *
 * @return CE_None on success, CE_Warning if no values returned,
 * CE_Failure if an error occurs.
 *
 * @since GDAL 3.2
 */

CPLErr GDALMDArray::GetStatistics(bool bApproxOK, bool bForce, double *pdfMin,
                                  double *pdfMax, double *pdfMean,
                                  double *pdfStdDev, GUInt64 *pnValidCount,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressData)
{
    if (!bForce)
        return CE_Warning;

    return ComputeStatistics(bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev,
                             pnValidCount, pfnProgress, pProgressData, nullptr)
               ? CE_None
               : CE_Failure;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

/**
 * \brief Compute statistics.
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this array.
 *
 * Pixels taken into account in statistics are those whose mask value
 * (as determined by GetMask()) is non-zero.
 *
 * Once computed, the statistics will generally be "set" back on the
 * owing dataset.
 *
 * Cached statistics can be cleared with GDALDataset::ClearStatistics().
 *
 * This method is the same as the C functions GDALMDArrayComputeStatistics().
 * and GDALMDArrayComputeStatisticsEx().
 *
 * @param bApproxOK Currently ignored. In the future, should be set to true
 * if statistics on the whole array are wished, or to false if a subset of it
 * may be used.
 *
 * @param pdfMin Location into which to load image minimum (may be NULL).
 *
 * @param pdfMax Location into which to load image maximum (may be NULL).-
 *
 * @param pdfMean Location into which to load image mean (may be NULL).
 *
 * @param pdfStdDev Location into which to load image standard deviation
 * (may be NULL).
 *
 * @param pnValidCount Number of samples whose value is different from the
 * nodata value. (may be NULL)
 *
 * @param pfnProgress a function to call to report progress, or NULL.
 *
 * @param pProgressData application data to pass to the progress function.
 *
 * @param papszOptions NULL-terminated list of options, of NULL. Added in 3.8.
 *                     Options are driver specific. For now the netCDF and Zarr
 *                     drivers recognize UPDATE_METADATA=YES, whose effect is
 *                     to add or update the actual_range attribute with the
 *                     computed min/max, only if done on the full array, in non
 *                     approximate mode, and the dataset is opened in update
 *                     mode.
 *
 * @return true on success
 *
 * @since GDAL 3.2
 */

bool GDALMDArray::ComputeStatistics(bool bApproxOK, double *pdfMin,
                                    double *pdfMax, double *pdfMean,
                                    double *pdfStdDev, GUInt64 *pnValidCount,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData,
                                    CSLConstList papszOptions)
{
    struct StatsPerChunkType
    {
        const GDALMDArray *array = nullptr;
        std::shared_ptr<GDALMDArray> poMask{};
        double dfMin = cpl::NumericLimits<double>::max();
        double dfMax = -cpl::NumericLimits<double>::max();
        double dfMean = 0.0;
        double dfM2 = 0.0;
        GUInt64 nValidCount = 0;
        std::vector<GByte> abyData{};
        std::vector<double> adfData{};
        std::vector<GByte> abyMaskData{};
        GDALProgressFunc pfnProgress = nullptr;
        void *pProgressData = nullptr;
    };

    const auto PerChunkFunc = [](GDALAbstractMDArray *,
                                 const GUInt64 *chunkArrayStartIdx,
                                 const size_t *chunkCount, GUInt64 iCurChunk,
                                 GUInt64 nChunkCount, void *pUserData)
    {
        StatsPerChunkType *data = static_cast<StatsPerChunkType *>(pUserData);
        const GDALMDArray *array = data->array;
        const GDALMDArray *poMask = data->poMask.get();
        const size_t nDims = array->GetDimensionCount();
        size_t nVals = 1;
        for (size_t i = 0; i < nDims; i++)
            nVals *= chunkCount[i];

        // Get mask
        data->abyMaskData.resize(nVals);
        if (!(poMask->Read(chunkArrayStartIdx, chunkCount, nullptr, nullptr,
                           poMask->GetDataType(), &data->abyMaskData[0])))
        {
            return false;
        }

        // Get data
        const auto &oType = array->GetDataType();
        if (oType.GetNumericDataType() == GDT_Float64)
        {
            data->adfData.resize(nVals);
            if (!array->Read(chunkArrayStartIdx, chunkCount, nullptr, nullptr,
                             oType, &data->adfData[0]))
            {
                return false;
            }
        }
        else
        {
            data->abyData.resize(nVals * oType.GetSize());
            if (!array->Read(chunkArrayStartIdx, chunkCount, nullptr, nullptr,
                             oType, &data->abyData[0]))
            {
                return false;
            }
            data->adfData.resize(nVals);
            GDALCopyWords64(&data->abyData[0], oType.GetNumericDataType(),
                            static_cast<int>(oType.GetSize()),
                            &data->adfData[0], GDT_Float64,
                            static_cast<int>(sizeof(double)),
                            static_cast<GPtrDiff_t>(nVals));
        }
        for (size_t i = 0; i < nVals; i++)
        {
            if (data->abyMaskData[i])
            {
                const double dfValue = data->adfData[i];
                data->dfMin = std::min(data->dfMin, dfValue);
                data->dfMax = std::max(data->dfMax, dfValue);
                data->nValidCount++;
                const double dfDelta = dfValue - data->dfMean;
                data->dfMean += dfDelta / data->nValidCount;
                data->dfM2 += dfDelta * (dfValue - data->dfMean);
            }
        }
        if (data->pfnProgress &&
            !data->pfnProgress(static_cast<double>(iCurChunk + 1) / nChunkCount,
                               "", data->pProgressData))
        {
            return false;
        }
        return true;
    };

    const auto &oType = GetDataType();
    if (oType.GetClass() != GEDTC_NUMERIC ||
        GDALDataTypeIsComplex(oType.GetNumericDataType()))
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Statistics can only be computed on non-complex numeric data type");
        return false;
    }

    const size_t nDims = GetDimensionCount();
    std::vector<GUInt64> arrayStartIdx(nDims);
    std::vector<GUInt64> count(nDims);
    const auto &poDims = GetDimensions();
    for (size_t i = 0; i < nDims; i++)
    {
        count[i] = poDims[i]->GetSize();
    }
    const char *pszSwathSize = CPLGetConfigOption("GDAL_SWATH_SIZE", nullptr);
    const size_t nMaxChunkSize =
        pszSwathSize
            ? static_cast<size_t>(
                  std::min(GIntBig(std::numeric_limits<size_t>::max() / 2),
                           CPLAtoGIntBig(pszSwathSize)))
            : static_cast<size_t>(
                  std::min(GIntBig(std::numeric_limits<size_t>::max() / 2),
                           GDALGetCacheMax64() / 4));
    StatsPerChunkType sData;
    sData.array = this;
    sData.poMask = GetMask(nullptr);
    if (sData.poMask == nullptr)
    {
        return false;
    }
    sData.pfnProgress = pfnProgress;
    sData.pProgressData = pProgressData;
    if (!ProcessPerChunk(arrayStartIdx.data(), count.data(),
                         GetProcessingChunkSize(nMaxChunkSize).data(),
                         PerChunkFunc, &sData))
    {
        return false;
    }

    if (pdfMin)
        *pdfMin = sData.dfMin;

    if (pdfMax)
        *pdfMax = sData.dfMax;

    if (pdfMean)
        *pdfMean = sData.dfMean;

    const double dfStdDev =
        sData.nValidCount > 0 ? sqrt(sData.dfM2 / sData.nValidCount) : 0.0;
    if (pdfStdDev)
        *pdfStdDev = dfStdDev;

    if (pnValidCount)
        *pnValidCount = sData.nValidCount;

    SetStatistics(bApproxOK, sData.dfMin, sData.dfMax, sData.dfMean, dfStdDev,
                  sData.nValidCount, papszOptions);

    return true;
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/
//! @cond Doxygen_Suppress
bool GDALMDArray::SetStatistics(bool /* bApproxStats */, double /* dfMin */,
                                double /* dfMax */, double /* dfMean */,
                                double /* dfStdDev */,
                                GUInt64 /* nValidCount */,
                                CSLConstList /* papszOptions */)
{
    CPLDebug("GDAL", "Cannot save statistics on a non-PAM MDArray");
    return false;
}

//! @endcond

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

/**
 * \brief Clear statistics.
 *
 * @since GDAL 3.4
 */
void GDALMDArray::ClearStatistics()
{
}

/************************************************************************/
/*                       GetCoordinateVariables()                       */
/************************************************************************/

/**
 * \brief Return coordinate variables.
 *
 * Coordinate variables are an alternate way of indexing an array that can
 * be sometimes used. For example, an array collected through remote sensing
 * might be indexed by (scanline, pixel). But there can be
 * a longitude and latitude arrays alongside that are also both indexed by
 * (scanline, pixel), and are referenced from operational arrays for
 * reprojection purposes.
 *
 * For netCDF, this will return the arrays referenced by the "coordinates"
 * attribute.
 *
 * This method is the same as the C function
 * GDALMDArrayGetCoordinateVariables().
 *
 * @return a vector of arrays
 *
 * @since GDAL 3.4
 */

std::vector<std::shared_ptr<GDALMDArray>>
GDALMDArray::GetCoordinateVariables() const
{
    return {};
}
