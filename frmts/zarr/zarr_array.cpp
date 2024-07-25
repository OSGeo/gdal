/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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

#include "zarr.h"
#include "ucs4_utf8.hpp"

#include "cpl_float.h"

#include "netcdf_cf_constants.h"  // for CF_UNITS, etc

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>

#if defined(__clang__) || defined(_MSC_VER)
#define COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT
#endif

namespace
{

inline std::vector<GByte> UTF8ToUCS4(const char *pszStr, bool needByteSwap)
{
    const size_t nLen = strlen(pszStr);
    // Worst case if that we need 4 more bytes than the UTF-8 one
    // (when the content is pure ASCII)
    if (nLen > std::numeric_limits<size_t>::max() / sizeof(uint32_t))
        throw std::bad_alloc();
    std::vector<GByte> ret(nLen * sizeof(uint32_t));
    size_t outPos = 0;
    for (size_t i = 0; i < nLen; outPos += sizeof(uint32_t))
    {
        uint32_t ucs4 = 0;
        int consumed = FcUtf8ToUcs4(
            reinterpret_cast<const uint8_t *>(pszStr + i), &ucs4, nLen - i);
        if (consumed <= 0)
        {
            ret.resize(outPos);
        }
        if (needByteSwap)
        {
            CPL_SWAP32PTR(&ucs4);
        }
        memcpy(&ret[outPos], &ucs4, sizeof(uint32_t));
        i += consumed;
    }
    ret.resize(outPos);
    return ret;
}

inline char *UCS4ToUTF8(const uint8_t *ucs4Ptr, size_t nSize, bool needByteSwap)
{
    // A UCS4 char can require up to 6 bytes in UTF8.
    if (nSize > (std::numeric_limits<size_t>::max() - 1) / 6 * 4)
        return nullptr;
    const size_t nOutSize = nSize / 4 * 6 + 1;
    char *ret = static_cast<char *>(VSI_MALLOC_VERBOSE(nOutSize));
    if (ret == nullptr)
        return nullptr;
    size_t outPos = 0;
    for (size_t i = 0; i + sizeof(uint32_t) - 1 < nSize; i += sizeof(uint32_t))
    {
        uint32_t ucs4;
        memcpy(&ucs4, ucs4Ptr + i, sizeof(uint32_t));
        if (needByteSwap)
        {
            CPL_SWAP32PTR(&ucs4);
        }
        int written =
            FcUcs4ToUtf8(ucs4, reinterpret_cast<uint8_t *>(ret + outPos));
        outPos += written;
    }
    ret[outPos] = 0;
    return ret;
}

}  // namespace

/************************************************************************/
/*                      ZarrArray::ParseChunkSize()                     */
/************************************************************************/

/* static */ bool ZarrArray::ParseChunkSize(const CPLJSONArray &oChunks,
                                            const GDALExtendedDataType &oType,
                                            std::vector<GUInt64> &anBlockSize)
{
    size_t nBlockSize = oType.GetSize();
    for (const auto &item : oChunks)
    {
        const auto nSize = static_cast<GUInt64>(item.ToLong());
        if (nSize == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid content for chunks");
            return false;
        }
        if (nBlockSize > std::numeric_limits<size_t>::max() / nSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large chunks");
            return false;
        }
        nBlockSize *= static_cast<size_t>(nSize);
        anBlockSize.emplace_back(nSize);
    }

    return true;
}

/************************************************************************/
/*                      ZarrArray::ComputeTileCount()                   */
/************************************************************************/

/* static */ uint64_t ZarrArray::ComputeTileCount(
    const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const std::vector<GUInt64> &anBlockSize)
{
    uint64_t nTotalTileCount = 1;
    for (size_t i = 0; i < aoDims.size(); ++i)
    {
        uint64_t nTileThisDim =
            (aoDims[i]->GetSize() / anBlockSize[i]) +
            (((aoDims[i]->GetSize() % anBlockSize[i]) != 0) ? 1 : 0);
        if (nTileThisDim != 0 &&
            nTotalTileCount >
                std::numeric_limits<uint64_t>::max() / nTileThisDim)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Array %s has more than 2^64 tiles. This is not supported.",
                osName.c_str());
            return 0;
        }
        nTotalTileCount *= nTileThisDim;
    }
    return nTotalTileCount;
}

/************************************************************************/
/*                         ZarrArray::ZarrArray()                       */
/************************************************************************/

ZarrArray::ZarrArray(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::vector<DtypeElt> &aoDtypeElts,
    const std::vector<GUInt64> &anBlockSize)
    :
#if !defined(COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT)
      GDALAbstractMDArray(osParentName, osName),
#endif
      GDALPamMDArray(osParentName, osName, poSharedResource->GetPAM()),
      m_poSharedResource(poSharedResource), m_aoDims(aoDims), m_oType(oType),
      m_aoDtypeElts(aoDtypeElts), m_anBlockSize(anBlockSize),
      m_oAttrGroup(m_osFullName, /*bContainerIsGroup=*/false)
{
    m_nTotalTileCount = ComputeTileCount(osName, aoDims, anBlockSize);
    if (m_nTotalTileCount == 0)
        return;

    // Compute individual tile size
    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;
    m_nTileSize = nSourceSize;
    for (const auto &nBlockSize : m_anBlockSize)
    {
        m_nTileSize *= static_cast<size_t>(nBlockSize);
    }

    m_bUseOptimizedCodePaths = CPLTestBool(
        CPLGetConfigOption("GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS", "YES"));
}

/************************************************************************/
/*                              ~ZarrArray()                            */
/************************************************************************/

ZarrArray::~ZarrArray()
{
    if (m_pabyNoData)
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
        CPLFree(m_pabyNoData);
    }

    DeallocateDecodedTileData();
}

/************************************************************************/
/*              ZarrArray::SerializeSpecialAttributes()                 */
/************************************************************************/

CPLJSONObject ZarrArray::SerializeSpecialAttributes()
{
    m_bSRSModified = false;
    m_oAttrGroup.UnsetModified();

    auto oAttrs = m_oAttrGroup.Serialize();

    if (m_poSRS)
    {
        CPLJSONObject oCRS;
        const char *const apszOptions[] = {"FORMAT=WKT2_2019", nullptr};
        char *pszWKT = nullptr;
        if (m_poSRS->exportToWkt(&pszWKT, apszOptions) == OGRERR_NONE)
        {
            oCRS.Add("wkt", pszWKT);
        }
        CPLFree(pszWKT);

        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            char *projjson = nullptr;
            if (m_poSRS->exportToPROJJSON(&projjson, nullptr) == OGRERR_NONE &&
                projjson != nullptr)
            {
                CPLJSONDocument oDocProjJSON;
                if (oDocProjJSON.LoadMemory(std::string(projjson)))
                {
                    oCRS.Add("projjson", oDocProjJSON.GetRoot());
                }
            }
            CPLFree(projjson);
        }

        const char *pszAuthorityCode = m_poSRS->GetAuthorityCode(nullptr);
        const char *pszAuthorityName = m_poSRS->GetAuthorityName(nullptr);
        if (pszAuthorityCode && pszAuthorityName &&
            EQUAL(pszAuthorityName, "EPSG"))
        {
            oCRS.Add("url",
                     std::string("http://www.opengis.net/def/crs/EPSG/0/") +
                         pszAuthorityCode);
        }

        oAttrs.Add(CRS_ATTRIBUTE_NAME, oCRS);
    }

    if (m_osUnit.empty())
    {
        if (m_bUnitModified)
            oAttrs.Delete(CF_UNITS);
    }
    else
    {
        oAttrs.Set(CF_UNITS, m_osUnit);
    }
    m_bUnitModified = false;

    if (!m_bHasOffset)
    {
        oAttrs.Delete(CF_ADD_OFFSET);
    }
    else
    {
        oAttrs.Set(CF_ADD_OFFSET, m_dfOffset);
    }
    m_bOffsetModified = false;

    if (!m_bHasScale)
    {
        oAttrs.Delete(CF_SCALE_FACTOR);
    }
    else
    {
        oAttrs.Set(CF_SCALE_FACTOR, m_dfScale);
    }
    m_bScaleModified = false;

    return oAttrs;
}

/************************************************************************/
/*                          FillBlockSize()                             */
/************************************************************************/

/* static */
bool ZarrArray::FillBlockSize(
    const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    const GDALExtendedDataType &oDataType, std::vector<GUInt64> &anBlockSize,
    CSLConstList papszOptions)
{
    const auto nDims = aoDimensions.size();
    anBlockSize.resize(nDims);
    for (size_t i = 0; i < nDims; ++i)
        anBlockSize[i] = 1;
    if (nDims >= 2)
    {
        anBlockSize[nDims - 2] =
            std::min(std::max<GUInt64>(1, aoDimensions[nDims - 2]->GetSize()),
                     static_cast<GUInt64>(256));
        anBlockSize[nDims - 1] =
            std::min(std::max<GUInt64>(1, aoDimensions[nDims - 1]->GetSize()),
                     static_cast<GUInt64>(256));
    }
    else if (nDims == 1)
    {
        anBlockSize[0] = std::max<GUInt64>(1, aoDimensions[0]->GetSize());
    }

    const char *pszBlockSize = CSLFetchNameValue(papszOptions, "BLOCKSIZE");
    if (pszBlockSize)
    {
        const auto aszTokens(
            CPLStringList(CSLTokenizeString2(pszBlockSize, ",", 0)));
        if (static_cast<size_t>(aszTokens.size()) != nDims)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of values in BLOCKSIZE");
            return false;
        }
        size_t nBlockSize = oDataType.GetSize();
        for (size_t i = 0; i < nDims; ++i)
        {
            anBlockSize[i] = static_cast<GUInt64>(CPLAtoGIntBig(aszTokens[i]));
            if (anBlockSize[i] == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Values in BLOCKSIZE should be > 0");
                return false;
            }
            if (anBlockSize[i] >
                std::numeric_limits<size_t>::max() / nBlockSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too large values in BLOCKSIZE");
                return false;
            }
            nBlockSize *= static_cast<size_t>(anBlockSize[i]);
        }
    }
    return true;
}

/************************************************************************/
/*                      DeallocateDecodedTileData()                     */
/************************************************************************/

void ZarrArray::DeallocateDecodedTileData()
{
    if (!m_abyDecodedTileData.empty())
    {
        const size_t nDTSize = m_oType.GetSize();
        GByte *pDst = &m_abyDecodedTileData[0];
        const size_t nValues = m_abyDecodedTileData.size() / nDTSize;
        for (const auto &elt : m_aoDtypeElts)
        {
            if (elt.nativeType == DtypeElt::NativeType::STRING_ASCII ||
                elt.nativeType == DtypeElt::NativeType::STRING_UNICODE)
            {
                for (size_t i = 0; i < nValues; i++, pDst += nDTSize)
                {
                    char *ptr;
                    char **pptr =
                        reinterpret_cast<char **>(pDst + elt.gdalOffset);
                    memcpy(&ptr, pptr, sizeof(ptr));
                    VSIFree(ptr);
                }
            }
        }
    }
}

/************************************************************************/
/*                             EncodeElt()                              */
/************************************************************************/

/* Encode from GDAL raw type to Zarr native type */
/*static*/
void ZarrArray::EncodeElt(const std::vector<DtypeElt> &elts, const GByte *pSrc,
                          GByte *pDst)
{
    for (const auto &elt : elts)
    {
        if (elt.nativeType == DtypeElt::NativeType::STRING_UNICODE)
        {
            const char *pStr =
                *reinterpret_cast<const char *const *>(pSrc + elt.gdalOffset);
            if (pStr)
            {
                try
                {
                    const auto ucs4 = UTF8ToUCS4(pStr, elt.needByteSwapping);
                    const auto ucs4Len = ucs4.size();
                    memcpy(pDst + elt.nativeOffset, ucs4.data(),
                           std::min(ucs4Len, elt.nativeSize));
                    if (ucs4Len > elt.nativeSize)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Too long string truncated");
                    }
                    else if (ucs4Len < elt.nativeSize)
                    {
                        memset(pDst + elt.nativeOffset + ucs4Len, 0,
                               elt.nativeSize - ucs4Len);
                    }
                }
                catch (const std::exception &)
                {
                    memset(pDst + elt.nativeOffset, 0, elt.nativeSize);
                }
            }
            else
            {
                memset(pDst + elt.nativeOffset, 0, elt.nativeSize);
            }
        }
        else if (elt.needByteSwapping)
        {
            if (elt.nativeSize == 2)
            {
                if (elt.gdalTypeIsApproxOfNative)
                {
                    CPLAssert(elt.nativeType == DtypeElt::NativeType::IEEEFP);
                    CPLAssert(elt.gdalType.GetNumericDataType() == GDT_Float32);
                    const uint32_t uint32Val =
                        *reinterpret_cast<const uint32_t *>(pSrc +
                                                            elt.gdalOffset);
                    bool bHasWarned = false;
                    uint16_t uint16Val =
                        CPL_SWAP16(CPLFloatToHalf(uint32Val, bHasWarned));
                    memcpy(pDst + elt.nativeOffset, &uint16Val,
                           sizeof(uint16Val));
                }
                else
                {
                    const uint16_t val =
                        CPL_SWAP16(*reinterpret_cast<const uint16_t *>(
                            pSrc + elt.gdalOffset));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                }
            }
            else if (elt.nativeSize == 4)
            {
                const uint32_t val = CPL_SWAP32(
                    *reinterpret_cast<const uint32_t *>(pSrc + elt.gdalOffset));
                memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
            }
            else if (elt.nativeSize == 8)
            {
                if (elt.nativeType == DtypeElt::NativeType::COMPLEX_IEEEFP)
                {
                    uint32_t val =
                        CPL_SWAP32(*reinterpret_cast<const uint32_t *>(
                            pSrc + elt.gdalOffset));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                    val = CPL_SWAP32(*reinterpret_cast<const uint32_t *>(
                        pSrc + elt.gdalOffset + 4));
                    memcpy(pDst + elt.nativeOffset + 4, &val, sizeof(val));
                }
                else
                {
                    const uint64_t val =
                        CPL_SWAP64(*reinterpret_cast<const uint64_t *>(
                            pSrc + elt.gdalOffset));
                    memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                }
            }
            else if (elt.nativeSize == 16)
            {
                uint64_t val = CPL_SWAP64(
                    *reinterpret_cast<const uint64_t *>(pSrc + elt.gdalOffset));
                memcpy(pDst + elt.nativeOffset, &val, sizeof(val));
                val = CPL_SWAP64(*reinterpret_cast<const uint64_t *>(
                    pSrc + elt.gdalOffset + 8));
                memcpy(pDst + elt.nativeOffset + 8, &val, sizeof(val));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if (elt.gdalTypeIsApproxOfNative)
        {
            if (elt.nativeType == DtypeElt::NativeType::IEEEFP &&
                elt.nativeSize == 2)
            {
                CPLAssert(elt.gdalType.GetNumericDataType() == GDT_Float32);
                const uint32_t uint32Val =
                    *reinterpret_cast<const uint32_t *>(pSrc + elt.gdalOffset);
                bool bHasWarned = false;
                const uint16_t uint16Val =
                    CPLFloatToHalf(uint32Val, bHasWarned);
                memcpy(pDst + elt.nativeOffset, &uint16Val, sizeof(uint16Val));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if (elt.nativeType == DtypeElt::NativeType::STRING_ASCII)
        {
            const char *pStr =
                *reinterpret_cast<const char *const *>(pSrc + elt.gdalOffset);
            if (pStr)
            {
                const size_t nLen = strlen(pStr);
                memcpy(pDst + elt.nativeOffset, pStr,
                       std::min(nLen, elt.nativeSize));
                if (nLen < elt.nativeSize)
                    memset(pDst + elt.nativeOffset + nLen, 0,
                           elt.nativeSize - nLen);
            }
            else
            {
                memset(pDst + elt.nativeOffset, 0, elt.nativeSize);
            }
        }
        else
        {
            CPLAssert(elt.nativeSize == elt.gdalSize);
            memcpy(pDst + elt.nativeOffset, pSrc + elt.gdalOffset,
                   elt.nativeSize);
        }
    }
}

/************************************************************************/
/*                ZarrArray::SerializeNumericNoData()                   */
/************************************************************************/

void ZarrArray::SerializeNumericNoData(CPLJSONObject &oRoot) const
{
    if (m_oType.GetNumericDataType() == GDT_Int64)
    {
        const auto nVal = GetNoDataValueAsInt64();
        oRoot.Add("fill_value", static_cast<GInt64>(nVal));
    }
    else if (m_oType.GetNumericDataType() == GDT_UInt64)
    {
        const auto nVal = GetNoDataValueAsUInt64();
        oRoot.Add("fill_value", static_cast<uint64_t>(nVal));
    }
    else
    {
        const double dfVal = GetNoDataValueAsDouble();
        if (std::isnan(dfVal))
            oRoot.Add("fill_value", "NaN");
        else if (dfVal == std::numeric_limits<double>::infinity())
            oRoot.Add("fill_value", "Infinity");
        else if (dfVal == -std::numeric_limits<double>::infinity())
            oRoot.Add("fill_value", "-Infinity");
        else if (GDALDataTypeIsInteger(m_oType.GetNumericDataType()))
            oRoot.Add("fill_value", static_cast<GInt64>(dfVal));
        else
            oRoot.Add("fill_value", dfVal);
    }
}

/************************************************************************/
/*                    ZarrArray::GetSpatialRef()                        */
/************************************************************************/

std::shared_ptr<OGRSpatialReference> ZarrArray::GetSpatialRef() const
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    if (m_poSRS)
        return m_poSRS;
    return GDALPamMDArray::GetSpatialRef();
}

/************************************************************************/
/*                        SetRawNoDataValue()                           */
/************************************************************************/

bool ZarrArray::SetRawNoDataValue(const void *pRawNoData)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Array opened in read-only mode");
        return false;
    }
    m_bDefinitionModified = true;
    RegisterNoDataValue(pRawNoData);
    return true;
}

/************************************************************************/
/*                        RegisterNoDataValue()                         */
/************************************************************************/

void ZarrArray::RegisterNoDataValue(const void *pNoData)
{
    if (m_pabyNoData)
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
    }

    if (pNoData == nullptr)
    {
        CPLFree(m_pabyNoData);
        m_pabyNoData = nullptr;
    }
    else
    {
        const auto nSize = m_oType.GetSize();
        if (m_pabyNoData == nullptr)
        {
            m_pabyNoData = static_cast<GByte *>(CPLMalloc(nSize));
        }
        memset(m_pabyNoData, 0, nSize);
        GDALExtendedDataType::CopyValue(pNoData, m_oType, m_pabyNoData,
                                        m_oType);
    }
}

/************************************************************************/
/*                      ZarrArray::BlockTranspose()                     */
/************************************************************************/

void ZarrArray::BlockTranspose(const ZarrByteVectorQuickResize &abySrc,
                               ZarrByteVectorQuickResize &abyDst,
                               bool bDecode) const
{
    // Perform transposition
    const size_t nDims = m_anBlockSize.size();
    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    struct Stack
    {
        size_t nIters = 0;
        const GByte *src_ptr = nullptr;
        GByte *dst_ptr = nullptr;
        size_t src_inc_offset = 0;
        size_t dst_inc_offset = 0;
    };

    std::vector<Stack> stack(nDims);
    stack.emplace_back(
        Stack());  // to make gcc 9.3 -O2 -Wnull-dereference happy

    if (bDecode)
    {
        stack[0].src_inc_offset = nSourceSize;
        for (size_t i = 1; i < nDims; ++i)
        {
            stack[i].src_inc_offset = stack[i - 1].src_inc_offset *
                                      static_cast<size_t>(m_anBlockSize[i - 1]);
        }

        stack[nDims - 1].dst_inc_offset = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            stack[i].dst_inc_offset = stack[i + 1].dst_inc_offset *
                                      static_cast<size_t>(m_anBlockSize[i + 1]);
        }
    }
    else
    {
        stack[0].dst_inc_offset = nSourceSize;
        for (size_t i = 1; i < nDims; ++i)
        {
            stack[i].dst_inc_offset = stack[i - 1].dst_inc_offset *
                                      static_cast<size_t>(m_anBlockSize[i - 1]);
        }

        stack[nDims - 1].src_inc_offset = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            stack[i].src_inc_offset = stack[i + 1].src_inc_offset *
                                      static_cast<size_t>(m_anBlockSize[i + 1]);
        }
    }

    stack[0].src_ptr = abySrc.data();
    stack[0].dst_ptr = &abyDst[0];

    size_t dimIdx = 0;
lbl_next_depth:
    if (dimIdx == nDims)
    {
        void *dst_ptr = stack[nDims].dst_ptr;
        const void *src_ptr = stack[nDims].src_ptr;
        if (nSourceSize == 1)
            *stack[nDims].dst_ptr = *stack[nDims].src_ptr;
        else if (nSourceSize == 2)
            *static_cast<uint16_t *>(dst_ptr) =
                *static_cast<const uint16_t *>(src_ptr);
        else if (nSourceSize == 4)
            *static_cast<uint32_t *>(dst_ptr) =
                *static_cast<const uint32_t *>(src_ptr);
        else if (nSourceSize == 8)
            *static_cast<uint64_t *>(dst_ptr) =
                *static_cast<const uint64_t *>(src_ptr);
        else
            memcpy(dst_ptr, src_ptr, nSourceSize);
    }
    else
    {
        stack[dimIdx].nIters = static_cast<size_t>(m_anBlockSize[dimIdx]);
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

/************************************************************************/
/*                        DecodeSourceElt()                             */
/************************************************************************/

/* static */
void ZarrArray::DecodeSourceElt(const std::vector<DtypeElt> &elts,
                                const GByte *pSrc, GByte *pDst)
{
    for (const auto &elt : elts)
    {
        if (elt.nativeType == DtypeElt::NativeType::STRING_UNICODE)
        {
            char *ptr;
            char **pDstPtr = reinterpret_cast<char **>(pDst + elt.gdalOffset);
            memcpy(&ptr, pDstPtr, sizeof(ptr));
            VSIFree(ptr);

            char *pDstStr = UCS4ToUTF8(pSrc + elt.nativeOffset, elt.nativeSize,
                                       elt.needByteSwapping);
            memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
        }
        else if (elt.needByteSwapping)
        {
            if (elt.nativeSize == 2)
            {
                uint16_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                if (elt.gdalTypeIsApproxOfNative)
                {
                    CPLAssert(elt.nativeType == DtypeElt::NativeType::IEEEFP);
                    CPLAssert(elt.gdalType.GetNumericDataType() == GDT_Float32);
                    uint32_t uint32Val = CPLHalfToFloat(CPL_SWAP16(val));
                    memcpy(pDst + elt.gdalOffset, &uint32Val,
                           sizeof(uint32Val));
                }
                else
                {
                    *reinterpret_cast<uint16_t *>(pDst + elt.gdalOffset) =
                        CPL_SWAP16(val);
                }
            }
            else if (elt.nativeSize == 4)
            {
                uint32_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                *reinterpret_cast<uint32_t *>(pDst + elt.gdalOffset) =
                    CPL_SWAP32(val);
            }
            else if (elt.nativeSize == 8)
            {
                if (elt.nativeType == DtypeElt::NativeType::COMPLEX_IEEEFP)
                {
                    uint32_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<uint32_t *>(pDst + elt.gdalOffset) =
                        CPL_SWAP32(val);
                    memcpy(&val, pSrc + elt.nativeOffset + 4, sizeof(val));
                    *reinterpret_cast<uint32_t *>(pDst + elt.gdalOffset + 4) =
                        CPL_SWAP32(val);
                }
                else
                {
                    uint64_t val;
                    memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                    *reinterpret_cast<uint64_t *>(pDst + elt.gdalOffset) =
                        CPL_SWAP64(val);
                }
            }
            else if (elt.nativeSize == 16)
            {
                uint64_t val;
                memcpy(&val, pSrc + elt.nativeOffset, sizeof(val));
                *reinterpret_cast<uint64_t *>(pDst + elt.gdalOffset) =
                    CPL_SWAP64(val);
                memcpy(&val, pSrc + elt.nativeOffset + 8, sizeof(val));
                *reinterpret_cast<uint64_t *>(pDst + elt.gdalOffset + 8) =
                    CPL_SWAP64(val);
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if (elt.gdalTypeIsApproxOfNative)
        {
            if (elt.nativeType == DtypeElt::NativeType::IEEEFP &&
                elt.nativeSize == 2)
            {
                CPLAssert(elt.gdalType.GetNumericDataType() == GDT_Float32);
                uint16_t uint16Val;
                memcpy(&uint16Val, pSrc + elt.nativeOffset, sizeof(uint16Val));
                uint32_t uint32Val = CPLHalfToFloat(uint16Val);
                memcpy(pDst + elt.gdalOffset, &uint32Val, sizeof(uint32Val));
            }
            else
            {
                CPLAssert(false);
            }
        }
        else if (elt.nativeType == DtypeElt::NativeType::STRING_ASCII)
        {
            char *ptr;
            char **pDstPtr = reinterpret_cast<char **>(pDst + elt.gdalOffset);
            memcpy(&ptr, pDstPtr, sizeof(ptr));
            VSIFree(ptr);

            char *pDstStr = static_cast<char *>(CPLMalloc(elt.nativeSize + 1));
            memcpy(pDstStr, pSrc + elt.nativeOffset, elt.nativeSize);
            pDstStr[elt.nativeSize] = 0;
            memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
        }
        else
        {
            CPLAssert(elt.nativeSize == elt.gdalSize);
            memcpy(pDst + elt.gdalOffset, pSrc + elt.nativeOffset,
                   elt.nativeSize);
        }
    }
}

/************************************************************************/
/*                  ZarrArray::IAdviseReadCommon()                      */
/************************************************************************/

bool ZarrArray::IAdviseReadCommon(const GUInt64 *arrayStartIdx,
                                  const size_t *count,
                                  CSLConstList papszOptions,
                                  std::vector<uint64_t> &anIndicesCur,
                                  int &nThreadsMax,
                                  std::vector<uint64_t> &anReqTilesIndices,
                                  size_t &nReqTiles) const
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    const size_t nDims = m_aoDims.size();
    anIndicesCur.resize(nDims);
    std::vector<uint64_t> anIndicesMin(nDims);
    std::vector<uint64_t> anIndicesMax(nDims);

    // Compute min and max tile indices in each dimension, and the total
    // nomber of tiles this represents.
    nReqTiles = 1;
    for (size_t i = 0; i < nDims; ++i)
    {
        anIndicesMin[i] = arrayStartIdx[i] / m_anBlockSize[i];
        anIndicesMax[i] = (arrayStartIdx[i] + count[i] - 1) / m_anBlockSize[i];
        // Overflow on number of tiles already checked in Create()
        nReqTiles *= static_cast<size_t>(anIndicesMax[i] - anIndicesMin[i] + 1);
    }

    // Find available cache size
    const size_t nCacheSize = [papszOptions]()
    {
        size_t nCacheSizeTmp;
        const char *pszCacheSize =
            CSLFetchNameValue(papszOptions, "CACHE_SIZE");
        if (pszCacheSize)
        {
            const auto nCacheSizeBig = CPLAtoGIntBig(pszCacheSize);
            if (nCacheSizeBig < 0 || static_cast<uint64_t>(nCacheSizeBig) >
                                         std::numeric_limits<size_t>::max() / 2)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Too big CACHE_SIZE");
                return std::numeric_limits<size_t>::max();
            }
            nCacheSizeTmp = static_cast<size_t>(nCacheSizeBig);
        }
        else
        {
            // Arbitrarily take half of remaining cache size
            nCacheSizeTmp = static_cast<size_t>(std::min(
                static_cast<uint64_t>(
                    (GDALGetCacheMax64() - GDALGetCacheUsed64()) / 2),
                static_cast<uint64_t>(std::numeric_limits<size_t>::max() / 2)));
            CPLDebug(ZARR_DEBUG_KEY, "Using implicit CACHE_SIZE=" CPL_FRMT_GUIB,
                     static_cast<GUIntBig>(nCacheSizeTmp));
        }
        return nCacheSizeTmp;
    }();
    if (nCacheSize == std::numeric_limits<size_t>::max())
        return false;

    // Check that cache size is sufficient to hold all needed tiles.
    // Also check that anReqTilesIndices size computation won't overflow.
    if (nReqTiles > nCacheSize / std::max(m_nTileSize, nDims))
    {
        CPLError(
            CE_Failure, CPLE_OutOfMemory,
            "CACHE_SIZE=" CPL_FRMT_GUIB " is not big enough to cache "
            "all needed tiles. "
            "At least " CPL_FRMT_GUIB " bytes would be needed",
            static_cast<GUIntBig>(nCacheSize),
            static_cast<GUIntBig>(nReqTiles * std::max(m_nTileSize, nDims)));
        return false;
    }

    const char *pszNumThreads = CSLFetchNameValueDef(
        papszOptions, "NUM_THREADS",
        CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS"));
    if (EQUAL(pszNumThreads, "ALL_CPUS"))
        nThreadsMax = CPLGetNumCPUs();
    else
        nThreadsMax = std::max(1, atoi(pszNumThreads));
    if (nThreadsMax > 1024)
        nThreadsMax = 1024;
    if (nThreadsMax <= 1)
        return true;
    CPLDebug(ZARR_DEBUG_KEY, "IAdviseRead(): Using up to %d threads",
             nThreadsMax);

    m_oMapTileIndexToCachedTile.clear();

    // Overflow checked above
    try
    {
        anReqTilesIndices.resize(nDims * nReqTiles);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate anReqTilesIndices: %s", e.what());
        return false;
    }

    size_t dimIdx = 0;
    size_t nTileIter = 0;
lbl_next_depth:
    if (dimIdx == nDims)
    {
        if (nDims == 2)
        {
            // optimize in common case
            memcpy(&anReqTilesIndices[nTileIter * nDims], anIndicesCur.data(),
                   sizeof(uint64_t) * 2);
        }
        else if (nDims == 3)
        {
            // optimize in common case
            memcpy(&anReqTilesIndices[nTileIter * nDims], anIndicesCur.data(),
                   sizeof(uint64_t) * 3);
        }
        else
        {
            memcpy(&anReqTilesIndices[nTileIter * nDims], anIndicesCur.data(),
                   sizeof(uint64_t) * nDims);
        }
        nTileIter++;
    }
    else
    {
        // This level of loop loops over blocks
        anIndicesCur[dimIdx] = anIndicesMin[dimIdx];
        while (true)
        {
            dimIdx++;
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if (anIndicesCur[dimIdx] == anIndicesMax[dimIdx])
                break;
            ++anIndicesCur[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;
    assert(nTileIter == nReqTiles);

    return true;
}

/************************************************************************/
/*                           ZarrArray::IRead()                         */
/************************************************************************/

bool ZarrArray::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                      const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                      const GDALExtendedDataType &bufferDataType,
                      void *pDstBuffer) const
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!AllocateWorkingBuffers())
        return false;

    // Need to be kept in top-level scope
    std::vector<GUInt64> arrayStartIdxMod;
    std::vector<GInt64> arrayStepMod;
    std::vector<GPtrDiff_t> bufferStrideMod;

    const size_t nDims = m_aoDims.size();
    bool negativeStep = false;
    for (size_t i = 0; i < nDims; ++i)
    {
        if (arrayStep[i] < 0)
        {
            negativeStep = true;
            break;
        }
    }

    // const auto eBufferDT = bufferDataType.GetNumericDataType();
    const auto nBufferDTSize = static_cast<int>(bufferDataType.GetSize());

    // Make sure that arrayStep[i] are positive for sake of simplicity
    if (negativeStep)
    {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        arrayStartIdxMod.resize(nDims);
        arrayStepMod.resize(nDims);
        bufferStrideMod.resize(nDims);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        for (size_t i = 0; i < nDims; ++i)
        {
            if (arrayStep[i] < 0)
            {
                arrayStartIdxMod[i] =
                    arrayStartIdx[i] - (count[i] - 1) * (-arrayStep[i]);
                arrayStepMod[i] = -arrayStep[i];
                bufferStrideMod[i] = -bufferStride[i];
                pDstBuffer =
                    static_cast<GByte *>(pDstBuffer) +
                    bufferStride[i] *
                        static_cast<GPtrDiff_t>(nBufferDTSize * (count[i] - 1));
            }
            else
            {
                arrayStartIdxMod[i] = arrayStartIdx[i];
                arrayStepMod[i] = arrayStep[i];
                bufferStrideMod[i] = bufferStride[i];
            }
        }
        arrayStartIdx = arrayStartIdxMod.data();
        arrayStep = arrayStepMod.data();
        bufferStride = bufferStrideMod.data();
    }

    std::vector<uint64_t> indicesOuterLoop(nDims + 1);
    std::vector<GByte *> dstPtrStackOuterLoop(nDims + 1);

    std::vector<uint64_t> indicesInnerLoop(nDims + 1);
    std::vector<GByte *> dstPtrStackInnerLoop(nDims + 1);

    std::vector<GPtrDiff_t> dstBufferStrideBytes;
    for (size_t i = 0; i < nDims; ++i)
    {
        dstBufferStrideBytes.push_back(bufferStride[i] *
                                       static_cast<GPtrDiff_t>(nBufferDTSize));
    }
    dstBufferStrideBytes.push_back(0);

    const auto nDTSize = m_oType.GetSize();

    std::vector<uint64_t> tileIndices(nDims);
    const size_t nSourceSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    std::vector<size_t> countInnerLoopInit(nDims + 1, 1);
    std::vector<size_t> countInnerLoop(nDims);

    const bool bBothAreNumericDT = m_oType.GetClass() == GEDTC_NUMERIC &&
                                   bufferDataType.GetClass() == GEDTC_NUMERIC;
    const bool bSameNumericDT =
        bBothAreNumericDT &&
        m_oType.GetNumericDataType() == bufferDataType.GetNumericDataType();
    const auto nSameDTSize = bSameNumericDT ? m_oType.GetSize() : 0;
    const bool bSameCompoundAndNoDynamicMem =
        m_oType.GetClass() == GEDTC_COMPOUND && m_oType == bufferDataType &&
        !m_oType.NeedsFreeDynamicMemory();
    std::vector<GByte> abyTargetNoData;
    bool bNoDataIsZero = false;

    size_t dimIdx = 0;
    dstPtrStackOuterLoop[0] = static_cast<GByte *>(pDstBuffer);
lbl_next_depth:
    if (dimIdx == nDims)
    {
        size_t dimIdxSubLoop = 0;
        dstPtrStackInnerLoop[0] = dstPtrStackOuterLoop[nDims];
        bool bEmptyTile = false;

        const GByte *pabySrcTile = m_abyDecodedTileData.empty()
                                       ? m_abyRawTileData.data()
                                       : m_abyDecodedTileData.data();
        bool bMatchFoundInMapTileIndexToCachedTile = false;

        // Use cache built by IAdviseRead() if possible
        if (!m_oMapTileIndexToCachedTile.empty())
        {
            uint64_t nTileIdx = 0;
            for (size_t j = 0; j < nDims; ++j)
            {
                if (j > 0)
                    nTileIdx *= m_aoDims[j - 1]->GetSize();
                nTileIdx += tileIndices[j];
            }
            const auto oIter = m_oMapTileIndexToCachedTile.find(nTileIdx);
            if (oIter != m_oMapTileIndexToCachedTile.end())
            {
                bMatchFoundInMapTileIndexToCachedTile = true;
                if (oIter->second.abyDecoded.empty())
                {
                    bEmptyTile = true;
                }
                else
                {
                    pabySrcTile = oIter->second.abyDecoded.data();
                }
            }
            else
            {
                CPLDebugOnly(ZARR_DEBUG_KEY,
                             "Cache miss for tile " CPL_FRMT_GUIB,
                             static_cast<GUIntBig>(nTileIdx));
            }
        }

        if (!bMatchFoundInMapTileIndexToCachedTile)
        {
            if (!tileIndices.empty() && tileIndices == m_anCachedTiledIndices)
            {
                if (!m_bCachedTiledValid)
                    return false;
                bEmptyTile = m_bCachedTiledEmpty;
            }
            else
            {
                if (!FlushDirtyTile())
                    return false;

                m_anCachedTiledIndices = tileIndices;
                m_bCachedTiledValid =
                    LoadTileData(tileIndices.data(), bEmptyTile);
                if (!m_bCachedTiledValid)
                {
                    return false;
                }
                m_bCachedTiledEmpty = bEmptyTile;
            }

            pabySrcTile = m_abyDecodedTileData.empty()
                              ? m_abyRawTileData.data()
                              : m_abyDecodedTileData.data();
        }
        const size_t nSrcDTSize =
            m_abyDecodedTileData.empty() ? nSourceSize : nDTSize;

        for (size_t i = 0; i < nDims; ++i)
        {
            countInnerLoopInit[i] = 1;
            if (arrayStep[i] != 0)
            {
                const auto nextBlockIdx =
                    std::min((1 + indicesOuterLoop[i] / m_anBlockSize[i]) *
                                 m_anBlockSize[i],
                             arrayStartIdx[i] + count[i] * arrayStep[i]);
                countInnerLoopInit[i] = static_cast<size_t>(
                    (nextBlockIdx - indicesOuterLoop[i] + arrayStep[i] - 1) /
                    arrayStep[i]);
            }
        }

        if (bEmptyTile && bBothAreNumericDT && abyTargetNoData.empty())
        {
            abyTargetNoData.resize(nBufferDTSize);
            if (m_pabyNoData)
            {
                GDALExtendedDataType::CopyValue(
                    m_pabyNoData, m_oType, &abyTargetNoData[0], bufferDataType);
                bNoDataIsZero = true;
                for (size_t i = 0; i < abyTargetNoData.size(); ++i)
                {
                    if (abyTargetNoData[i] != 0)
                        bNoDataIsZero = false;
                }
            }
            else
            {
                bNoDataIsZero = true;
                GByte zero = 0;
                GDALCopyWords(&zero, GDT_Byte, 0, &abyTargetNoData[0],
                              bufferDataType.GetNumericDataType(), 0, 1);
            }
        }

    lbl_next_depth_inner_loop:
        if (nDims == 0 || dimIdxSubLoop == nDims - 1)
        {
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            void *dst_ptr = dstPtrStackInnerLoop[dimIdxSubLoop];

            if (m_bUseOptimizedCodePaths && bEmptyTile && bBothAreNumericDT &&
                bNoDataIsZero &&
                nBufferDTSize == dstBufferStrideBytes[dimIdxSubLoop])
            {
                memset(dst_ptr, 0,
                       nBufferDTSize * countInnerLoopInit[dimIdxSubLoop]);
                goto end_inner_loop;
            }
            else if (m_bUseOptimizedCodePaths && bEmptyTile &&
                     !abyTargetNoData.empty() && bBothAreNumericDT &&
                     dstBufferStrideBytes[dimIdxSubLoop] <
                         std::numeric_limits<int>::max())
            {
                GDALCopyWords64(
                    abyTargetNoData.data(), bufferDataType.GetNumericDataType(),
                    0, dst_ptr, bufferDataType.GetNumericDataType(),
                    static_cast<int>(dstBufferStrideBytes[dimIdxSubLoop]),
                    static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]));
                goto end_inner_loop;
            }
            else if (bEmptyTile)
            {
                for (size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                     ++i, dst_ptr = static_cast<uint8_t *>(dst_ptr) +
                                    dstBufferStrideBytes[dimIdxSubLoop])
                {
                    if (bNoDataIsZero)
                    {
                        if (nBufferDTSize == 1)
                        {
                            *static_cast<uint8_t *>(dst_ptr) = 0;
                        }
                        else if (nBufferDTSize == 2)
                        {
                            *static_cast<uint16_t *>(dst_ptr) = 0;
                        }
                        else if (nBufferDTSize == 4)
                        {
                            *static_cast<uint32_t *>(dst_ptr) = 0;
                        }
                        else if (nBufferDTSize == 8)
                        {
                            *static_cast<uint64_t *>(dst_ptr) = 0;
                        }
                        else if (nBufferDTSize == 16)
                        {
                            static_cast<uint64_t *>(dst_ptr)[0] = 0;
                            static_cast<uint64_t *>(dst_ptr)[1] = 0;
                        }
                        else
                        {
                            CPLAssert(false);
                        }
                    }
                    else if (m_pabyNoData)
                    {
                        if (bBothAreNumericDT)
                        {
                            const void *src_ptr_v = abyTargetNoData.data();
                            if (nBufferDTSize == 1)
                                *static_cast<uint8_t *>(dst_ptr) =
                                    *static_cast<const uint8_t *>(src_ptr_v);
                            else if (nBufferDTSize == 2)
                                *static_cast<uint16_t *>(dst_ptr) =
                                    *static_cast<const uint16_t *>(src_ptr_v);
                            else if (nBufferDTSize == 4)
                                *static_cast<uint32_t *>(dst_ptr) =
                                    *static_cast<const uint32_t *>(src_ptr_v);
                            else if (nBufferDTSize == 8)
                                *static_cast<uint64_t *>(dst_ptr) =
                                    *static_cast<const uint64_t *>(src_ptr_v);
                            else if (nBufferDTSize == 16)
                            {
                                static_cast<uint64_t *>(dst_ptr)[0] =
                                    static_cast<const uint64_t *>(src_ptr_v)[0];
                                static_cast<uint64_t *>(dst_ptr)[1] =
                                    static_cast<const uint64_t *>(src_ptr_v)[1];
                            }
                            else
                            {
                                CPLAssert(false);
                            }
                        }
                        else
                        {
                            GDALExtendedDataType::CopyValue(
                                m_pabyNoData, m_oType, dst_ptr, bufferDataType);
                        }
                    }
                    else
                    {
                        memset(dst_ptr, 0, nBufferDTSize);
                    }
                }

                goto end_inner_loop;
            }

            size_t nOffset = 0;
            for (size_t i = 0; i < nDims; i++)
            {
                nOffset = static_cast<size_t>(
                    nOffset * m_anBlockSize[i] +
                    (indicesInnerLoop[i] - tileIndices[i] * m_anBlockSize[i]));
            }
            const GByte *src_ptr = pabySrcTile + nOffset * nSrcDTSize;
            const auto step = nDims == 0 ? 0 : arrayStep[dimIdxSubLoop];

            if (m_bUseOptimizedCodePaths && bBothAreNumericDT &&
                step <= static_cast<GIntBig>(std::numeric_limits<int>::max() /
                                             nDTSize) &&
                dstBufferStrideBytes[dimIdxSubLoop] <=
                    std::numeric_limits<int>::max())
            {
                GDALCopyWords64(
                    src_ptr, m_oType.GetNumericDataType(),
                    static_cast<int>(step * nDTSize), dst_ptr,
                    bufferDataType.GetNumericDataType(),
                    static_cast<int>(dstBufferStrideBytes[dimIdxSubLoop]),
                    static_cast<GPtrDiff_t>(countInnerLoopInit[dimIdxSubLoop]));

                goto end_inner_loop;
            }

            for (size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                 ++i, src_ptr += step * nSrcDTSize,
                        dst_ptr = static_cast<uint8_t *>(dst_ptr) +
                                  dstBufferStrideBytes[dimIdxSubLoop])
            {
                if (bSameNumericDT)
                {
                    const void *src_ptr_v = src_ptr;
                    if (nSameDTSize == 1)
                        *static_cast<uint8_t *>(dst_ptr) =
                            *static_cast<const uint8_t *>(src_ptr_v);
                    else if (nSameDTSize == 2)
                    {
                        *static_cast<uint16_t *>(dst_ptr) =
                            *static_cast<const uint16_t *>(src_ptr_v);
                    }
                    else if (nSameDTSize == 4)
                    {
                        *static_cast<uint32_t *>(dst_ptr) =
                            *static_cast<const uint32_t *>(src_ptr_v);
                    }
                    else if (nSameDTSize == 8)
                    {
                        *static_cast<uint64_t *>(dst_ptr) =
                            *static_cast<const uint64_t *>(src_ptr_v);
                    }
                    else if (nSameDTSize == 16)
                    {
                        static_cast<uint64_t *>(dst_ptr)[0] =
                            static_cast<const uint64_t *>(src_ptr_v)[0];
                        static_cast<uint64_t *>(dst_ptr)[1] =
                            static_cast<const uint64_t *>(src_ptr_v)[1];
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if (bSameCompoundAndNoDynamicMem)
                {
                    memcpy(dst_ptr, src_ptr, nDTSize);
                }
                else if (m_oType.GetClass() == GEDTC_STRING)
                {
                    if (m_aoDtypeElts.back().nativeType ==
                        DtypeElt::NativeType::STRING_UNICODE)
                    {
                        char *pDstStr =
                            UCS4ToUTF8(src_ptr, nSourceSize,
                                       m_aoDtypeElts.back().needByteSwapping);
                        char **pDstPtr = static_cast<char **>(dst_ptr);
                        memcpy(pDstPtr, &pDstStr, sizeof(pDstStr));
                    }
                    else
                    {
                        char *pDstStr =
                            static_cast<char *>(CPLMalloc(nSourceSize + 1));
                        memcpy(pDstStr, src_ptr, nSourceSize);
                        pDstStr[nSourceSize] = 0;
                        char **pDstPtr = static_cast<char **>(dst_ptr);
                        memcpy(pDstPtr, &pDstStr, sizeof(char *));
                    }
                }
                else
                {
                    GDALExtendedDataType::CopyValue(src_ptr, m_oType, dst_ptr,
                                                    bufferDataType);
                }
            }
        }
        else
        {
            // This level of loop loops over individual samples, within a
            // block
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            countInnerLoop[dimIdxSubLoop] = countInnerLoopInit[dimIdxSubLoop];
            while (true)
            {
                dimIdxSubLoop++;
                dstPtrStackInnerLoop[dimIdxSubLoop] =
                    dstPtrStackInnerLoop[dimIdxSubLoop - 1];
                goto lbl_next_depth_inner_loop;
            lbl_return_to_caller_inner_loop:
                dimIdxSubLoop--;
                --countInnerLoop[dimIdxSubLoop];
                if (countInnerLoop[dimIdxSubLoop] == 0)
                {
                    break;
                }
                indicesInnerLoop[dimIdxSubLoop] += arrayStep[dimIdxSubLoop];
                dstPtrStackInnerLoop[dimIdxSubLoop] +=
                    dstBufferStrideBytes[dimIdxSubLoop];
            }
        }
    end_inner_loop:
        if (dimIdxSubLoop > 0)
            goto lbl_return_to_caller_inner_loop;
    }
    else
    {
        // This level of loop loops over blocks
        indicesOuterLoop[dimIdx] = arrayStartIdx[dimIdx];
        tileIndices[dimIdx] = indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        while (true)
        {
            dimIdx++;
            dstPtrStackOuterLoop[dimIdx] = dstPtrStackOuterLoop[dimIdx - 1];
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if (count[dimIdx] == 1 || arrayStep[dimIdx] == 0)
                break;

            size_t nIncr;
            if (static_cast<GUInt64>(arrayStep[dimIdx]) < m_anBlockSize[dimIdx])
            {
                // Compute index at next block boundary
                auto newIdx =
                    indicesOuterLoop[dimIdx] +
                    (m_anBlockSize[dimIdx] -
                     (indicesOuterLoop[dimIdx] % m_anBlockSize[dimIdx]));
                // And round up compared to arrayStartIdx, arrayStep
                nIncr = static_cast<size_t>((newIdx - indicesOuterLoop[dimIdx] +
                                             arrayStep[dimIdx] - 1) /
                                            arrayStep[dimIdx]);
            }
            else
            {
                nIncr = 1;
            }
            indicesOuterLoop[dimIdx] += nIncr * arrayStep[dimIdx];
            if (indicesOuterLoop[dimIdx] >
                arrayStartIdx[dimIdx] + (count[dimIdx] - 1) * arrayStep[dimIdx])
                break;
            dstPtrStackOuterLoop[dimIdx] +=
                bufferStride[dimIdx] *
                static_cast<GPtrDiff_t>(nIncr * nBufferDTSize);
            tileIndices[dimIdx] =
                indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                           ZarrArray::IRead()                         */
/************************************************************************/

bool ZarrArray::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                       const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                       const GDALExtendedDataType &bufferDataType,
                       const void *pSrcBuffer)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!AllocateWorkingBuffers())
        return false;

    m_oMapTileIndexToCachedTile.clear();

    // Need to be kept in top-level scope
    std::vector<GUInt64> arrayStartIdxMod;
    std::vector<GInt64> arrayStepMod;
    std::vector<GPtrDiff_t> bufferStrideMod;

    const size_t nDims = m_aoDims.size();
    bool negativeStep = false;
    bool bWriteWholeTileInit = true;
    for (size_t i = 0; i < nDims; ++i)
    {
        if (arrayStep[i] < 0)
        {
            negativeStep = true;
            if (arrayStep[i] != -1 && count[i] > 1)
                bWriteWholeTileInit = false;
        }
        else if (arrayStep[i] != 1 && count[i] > 1)
            bWriteWholeTileInit = false;
    }

    const auto nBufferDTSize = static_cast<int>(bufferDataType.GetSize());

    // Make sure that arrayStep[i] are positive for sake of simplicity
    if (negativeStep)
    {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        arrayStartIdxMod.resize(nDims);
        arrayStepMod.resize(nDims);
        bufferStrideMod.resize(nDims);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        for (size_t i = 0; i < nDims; ++i)
        {
            if (arrayStep[i] < 0)
            {
                arrayStartIdxMod[i] =
                    arrayStartIdx[i] - (count[i] - 1) * (-arrayStep[i]);
                arrayStepMod[i] = -arrayStep[i];
                bufferStrideMod[i] = -bufferStride[i];
                pSrcBuffer =
                    static_cast<const GByte *>(pSrcBuffer) +
                    bufferStride[i] *
                        static_cast<GPtrDiff_t>(nBufferDTSize * (count[i] - 1));
            }
            else
            {
                arrayStartIdxMod[i] = arrayStartIdx[i];
                arrayStepMod[i] = arrayStep[i];
                bufferStrideMod[i] = bufferStride[i];
            }
        }
        arrayStartIdx = arrayStartIdxMod.data();
        arrayStep = arrayStepMod.data();
        bufferStride = bufferStrideMod.data();
    }

    std::vector<uint64_t> indicesOuterLoop(nDims + 1);
    std::vector<const GByte *> srcPtrStackOuterLoop(nDims + 1);

    std::vector<size_t> offsetDstBuffer(nDims + 1);
    std::vector<const GByte *> srcPtrStackInnerLoop(nDims + 1);

    std::vector<GPtrDiff_t> srcBufferStrideBytes;
    for (size_t i = 0; i < nDims; ++i)
    {
        srcBufferStrideBytes.push_back(bufferStride[i] *
                                       static_cast<GPtrDiff_t>(nBufferDTSize));
    }
    srcBufferStrideBytes.push_back(0);

    const auto nDTSize = m_oType.GetSize();

    std::vector<uint64_t> tileIndices(nDims);
    const size_t nNativeSize =
        m_aoDtypeElts.back().nativeOffset + m_aoDtypeElts.back().nativeSize;

    std::vector<size_t> countInnerLoopInit(nDims + 1, 1);
    std::vector<size_t> countInnerLoop(nDims);

    const bool bBothAreNumericDT = m_oType.GetClass() == GEDTC_NUMERIC &&
                                   bufferDataType.GetClass() == GEDTC_NUMERIC;
    const bool bSameNumericDT =
        bBothAreNumericDT &&
        m_oType.GetNumericDataType() == bufferDataType.GetNumericDataType();
    const auto nSameDTSize = bSameNumericDT ? m_oType.GetSize() : 0;
    const bool bSameCompoundAndNoDynamicMem =
        m_oType.GetClass() == GEDTC_COMPOUND && m_oType == bufferDataType &&
        !m_oType.NeedsFreeDynamicMemory();

    size_t dimIdx = 0;
    size_t dimIdxForCopy = nDims == 0 ? 0 : nDims - 1;
    if (nDims)
    {
        while (dimIdxForCopy > 0 && count[dimIdxForCopy] == 1)
            --dimIdxForCopy;
    }

    srcPtrStackOuterLoop[0] = static_cast<const GByte *>(pSrcBuffer);
lbl_next_depth:
    if (dimIdx == nDims)
    {
        bool bWriteWholeTile = bWriteWholeTileInit;
        bool bPartialTile = false;
        for (size_t i = 0; i < nDims; ++i)
        {
            countInnerLoopInit[i] = 1;
            if (arrayStep[i] != 0)
            {
                const auto nextBlockIdx =
                    std::min((1 + indicesOuterLoop[i] / m_anBlockSize[i]) *
                                 m_anBlockSize[i],
                             arrayStartIdx[i] + count[i] * arrayStep[i]);
                countInnerLoopInit[i] = static_cast<size_t>(
                    (nextBlockIdx - indicesOuterLoop[i] + arrayStep[i] - 1) /
                    arrayStep[i]);
            }
            if (bWriteWholeTile)
            {
                const bool bWholePartialTileThisDim =
                    indicesOuterLoop[i] == 0 &&
                    countInnerLoopInit[i] == m_aoDims[i]->GetSize();
                bWriteWholeTile = (countInnerLoopInit[i] == m_anBlockSize[i] ||
                                   bWholePartialTileThisDim);
                if (bWholePartialTileThisDim)
                {
                    bPartialTile = true;
                }
            }
        }

        size_t dimIdxSubLoop = 0;
        srcPtrStackInnerLoop[0] = srcPtrStackOuterLoop[nDims];
        const size_t nCacheDTSize =
            m_abyDecodedTileData.empty() ? nNativeSize : nDTSize;
        auto &abyTile = m_abyDecodedTileData.empty() ? m_abyRawTileData
                                                     : m_abyDecodedTileData;

        if (!tileIndices.empty() && tileIndices == m_anCachedTiledIndices)
        {
            if (!m_bCachedTiledValid)
                return false;
        }
        else
        {
            if (!FlushDirtyTile())
                return false;

            m_anCachedTiledIndices = tileIndices;
            m_bCachedTiledValid = true;

            if (bWriteWholeTile)
            {
                if (bPartialTile)
                {
                    DeallocateDecodedTileData();
                    memset(&abyTile[0], 0, abyTile.size());
                }
            }
            else
            {
                // If we don't write the whole tile, we need to fetch a
                // potentially existing one.
                bool bEmptyTile = false;
                m_bCachedTiledValid =
                    LoadTileData(tileIndices.data(), bEmptyTile);
                if (!m_bCachedTiledValid)
                {
                    return false;
                }

                if (bEmptyTile)
                {
                    DeallocateDecodedTileData();

                    if (m_pabyNoData == nullptr)
                    {
                        memset(&abyTile[0], 0, abyTile.size());
                    }
                    else
                    {
                        const size_t nElts = abyTile.size() / nCacheDTSize;
                        GByte *dstPtr = &abyTile[0];
                        if (m_oType.GetClass() == GEDTC_NUMERIC)
                        {
                            GDALCopyWords64(
                                m_pabyNoData, m_oType.GetNumericDataType(), 0,
                                dstPtr, m_oType.GetNumericDataType(),
                                static_cast<int>(m_oType.GetSize()),
                                static_cast<GPtrDiff_t>(nElts));
                        }
                        else
                        {
                            for (size_t i = 0; i < nElts; ++i)
                            {
                                GDALExtendedDataType::CopyValue(
                                    m_pabyNoData, m_oType, dstPtr, m_oType);
                                dstPtr += nCacheDTSize;
                            }
                        }
                    }
                }
            }
        }
        m_bDirtyTile = true;
        m_bCachedTiledEmpty = false;
        if (nDims)
            offsetDstBuffer[0] = static_cast<size_t>(
                indicesOuterLoop[0] - tileIndices[0] * m_anBlockSize[0]);

        GByte *pabyTile = &abyTile[0];

    lbl_next_depth_inner_loop:
        if (dimIdxSubLoop == dimIdxForCopy)
        {
            size_t nOffset = offsetDstBuffer[dimIdxSubLoop];
            GInt64 step = nDims == 0 ? 0 : arrayStep[dimIdxSubLoop];
            for (size_t i = dimIdxSubLoop + 1; i < nDims; ++i)
            {
                nOffset = static_cast<size_t>(
                    nOffset * m_anBlockSize[i] +
                    (indicesOuterLoop[i] - tileIndices[i] * m_anBlockSize[i]));
                step *= m_anBlockSize[i];
            }
            const void *src_ptr = srcPtrStackInnerLoop[dimIdxSubLoop];
            GByte *dst_ptr = pabyTile + nOffset * nCacheDTSize;

            if (m_bUseOptimizedCodePaths && bBothAreNumericDT)
            {
                if (countInnerLoopInit[dimIdxSubLoop] == 1 && bSameNumericDT)
                {
                    void *dst_ptr_v = dst_ptr;
                    if (nSameDTSize == 1)
                        *static_cast<uint8_t *>(dst_ptr_v) =
                            *static_cast<const uint8_t *>(src_ptr);
                    else if (nSameDTSize == 2)
                    {
                        *static_cast<uint16_t *>(dst_ptr_v) =
                            *static_cast<const uint16_t *>(src_ptr);
                    }
                    else if (nSameDTSize == 4)
                    {
                        *static_cast<uint32_t *>(dst_ptr_v) =
                            *static_cast<const uint32_t *>(src_ptr);
                    }
                    else if (nSameDTSize == 8)
                    {
                        *static_cast<uint64_t *>(dst_ptr_v) =
                            *static_cast<const uint64_t *>(src_ptr);
                    }
                    else if (nSameDTSize == 16)
                    {
                        static_cast<uint64_t *>(dst_ptr_v)[0] =
                            static_cast<const uint64_t *>(src_ptr)[0];
                        static_cast<uint64_t *>(dst_ptr_v)[1] =
                            static_cast<const uint64_t *>(src_ptr)[1];
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if (step <=
                             static_cast<GIntBig>(
                                 std::numeric_limits<int>::max() / nDTSize) &&
                         srcBufferStrideBytes[dimIdxSubLoop] <=
                             std::numeric_limits<int>::max())
                {
                    GDALCopyWords64(
                        src_ptr, bufferDataType.GetNumericDataType(),
                        static_cast<int>(srcBufferStrideBytes[dimIdxSubLoop]),
                        dst_ptr, m_oType.GetNumericDataType(),
                        static_cast<int>(step * nDTSize),
                        static_cast<GPtrDiff_t>(
                            countInnerLoopInit[dimIdxSubLoop]));
                }
                goto end_inner_loop;
            }

            for (size_t i = 0; i < countInnerLoopInit[dimIdxSubLoop];
                 ++i, dst_ptr += step * nCacheDTSize,
                        src_ptr = static_cast<const uint8_t *>(src_ptr) +
                                  srcBufferStrideBytes[dimIdxSubLoop])
            {
                if (bSameNumericDT)
                {
                    void *dst_ptr_v = dst_ptr;
                    if (nSameDTSize == 1)
                        *static_cast<uint8_t *>(dst_ptr_v) =
                            *static_cast<const uint8_t *>(src_ptr);
                    else if (nSameDTSize == 2)
                    {
                        *static_cast<uint16_t *>(dst_ptr_v) =
                            *static_cast<const uint16_t *>(src_ptr);
                    }
                    else if (nSameDTSize == 4)
                    {
                        *static_cast<uint32_t *>(dst_ptr_v) =
                            *static_cast<const uint32_t *>(src_ptr);
                    }
                    else if (nSameDTSize == 8)
                    {
                        *static_cast<uint64_t *>(dst_ptr_v) =
                            *static_cast<const uint64_t *>(src_ptr);
                    }
                    else if (nSameDTSize == 16)
                    {
                        static_cast<uint64_t *>(dst_ptr_v)[0] =
                            static_cast<const uint64_t *>(src_ptr)[0];
                        static_cast<uint64_t *>(dst_ptr_v)[1] =
                            static_cast<const uint64_t *>(src_ptr)[1];
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
                else if (bSameCompoundAndNoDynamicMem)
                {
                    memcpy(dst_ptr, src_ptr, nDTSize);
                }
                else if (m_oType.GetClass() == GEDTC_STRING)
                {
                    const char *pSrcStr =
                        *static_cast<const char *const *>(src_ptr);
                    if (pSrcStr)
                    {
                        const size_t nLen = strlen(pSrcStr);
                        if (m_aoDtypeElts.back().nativeType ==
                            DtypeElt::NativeType::STRING_UNICODE)
                        {
                            try
                            {
                                const auto ucs4 = UTF8ToUCS4(
                                    pSrcStr,
                                    m_aoDtypeElts.back().needByteSwapping);
                                const auto ucs4Len = ucs4.size();
                                memcpy(dst_ptr, ucs4.data(),
                                       std::min(ucs4Len, nNativeSize));
                                if (ucs4Len > nNativeSize)
                                {
                                    CPLError(CE_Warning, CPLE_AppDefined,
                                             "Too long string truncated");
                                }
                                else if (ucs4Len < nNativeSize)
                                {
                                    memset(dst_ptr + ucs4Len, 0,
                                           nNativeSize - ucs4Len);
                                }
                            }
                            catch (const std::exception &)
                            {
                                memset(dst_ptr, 0, nNativeSize);
                            }
                        }
                        else
                        {
                            memcpy(dst_ptr, pSrcStr,
                                   std::min(nLen, nNativeSize));
                            if (nLen < nNativeSize)
                                memset(dst_ptr + nLen, 0, nNativeSize - nLen);
                        }
                    }
                    else
                    {
                        memset(dst_ptr, 0, nNativeSize);
                    }
                }
                else
                {
                    if (m_oType.NeedsFreeDynamicMemory())
                        m_oType.FreeDynamicMemory(dst_ptr);
                    GDALExtendedDataType::CopyValue(src_ptr, bufferDataType,
                                                    dst_ptr, m_oType);
                }
            }
        }
        else
        {
            // This level of loop loops over individual samples, within a
            // block
            countInnerLoop[dimIdxSubLoop] = countInnerLoopInit[dimIdxSubLoop];
            while (true)
            {
                dimIdxSubLoop++;
                srcPtrStackInnerLoop[dimIdxSubLoop] =
                    srcPtrStackInnerLoop[dimIdxSubLoop - 1];
                offsetDstBuffer[dimIdxSubLoop] =
                    static_cast<size_t>(offsetDstBuffer[dimIdxSubLoop - 1] *
                                            m_anBlockSize[dimIdxSubLoop] +
                                        (indicesOuterLoop[dimIdxSubLoop] -
                                         tileIndices[dimIdxSubLoop] *
                                             m_anBlockSize[dimIdxSubLoop]));
                goto lbl_next_depth_inner_loop;
            lbl_return_to_caller_inner_loop:
                dimIdxSubLoop--;
                --countInnerLoop[dimIdxSubLoop];
                if (countInnerLoop[dimIdxSubLoop] == 0)
                {
                    break;
                }
                srcPtrStackInnerLoop[dimIdxSubLoop] +=
                    srcBufferStrideBytes[dimIdxSubLoop];
                offsetDstBuffer[dimIdxSubLoop] +=
                    static_cast<size_t>(arrayStep[dimIdxSubLoop]);
            }
        }
    end_inner_loop:
        if (dimIdxSubLoop > 0)
            goto lbl_return_to_caller_inner_loop;
    }
    else
    {
        // This level of loop loops over blocks
        indicesOuterLoop[dimIdx] = arrayStartIdx[dimIdx];
        tileIndices[dimIdx] = indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        while (true)
        {
            dimIdx++;
            srcPtrStackOuterLoop[dimIdx] = srcPtrStackOuterLoop[dimIdx - 1];
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if (count[dimIdx] == 1 || arrayStep[dimIdx] == 0)
                break;

            size_t nIncr;
            if (static_cast<GUInt64>(arrayStep[dimIdx]) < m_anBlockSize[dimIdx])
            {
                // Compute index at next block boundary
                auto newIdx =
                    indicesOuterLoop[dimIdx] +
                    (m_anBlockSize[dimIdx] -
                     (indicesOuterLoop[dimIdx] % m_anBlockSize[dimIdx]));
                // And round up compared to arrayStartIdx, arrayStep
                nIncr = static_cast<size_t>((newIdx - indicesOuterLoop[dimIdx] +
                                             arrayStep[dimIdx] - 1) /
                                            arrayStep[dimIdx]);
            }
            else
            {
                nIncr = 1;
            }
            indicesOuterLoop[dimIdx] += nIncr * arrayStep[dimIdx];
            if (indicesOuterLoop[dimIdx] >
                arrayStartIdx[dimIdx] + (count[dimIdx] - 1) * arrayStep[dimIdx])
                break;
            srcPtrStackOuterLoop[dimIdx] +=
                bufferStride[dimIdx] *
                static_cast<GPtrDiff_t>(nIncr * nBufferDTSize);
            tileIndices[dimIdx] =
                indicesOuterLoop[dimIdx] / m_anBlockSize[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                   ZarrArray::IsEmptyTile()                           */
/************************************************************************/

bool ZarrArray::IsEmptyTile(const ZarrByteVectorQuickResize &abyTile) const
{
    if (m_pabyNoData == nullptr || (m_oType.GetClass() == GEDTC_NUMERIC &&
                                    GetNoDataValueAsDouble() == 0.0))
    {
        const size_t nBytes = abyTile.size();
        size_t i = 0;
        for (; i + (sizeof(size_t) - 1) < nBytes; i += sizeof(size_t))
        {
            if (*reinterpret_cast<const size_t *>(abyTile.data() + i) != 0)
            {
                return false;
            }
        }
        for (; i < nBytes; ++i)
        {
            if (abyTile[i] != 0)
            {
                return false;
            }
        }
        return true;
    }
    else if (m_oType.GetClass() == GEDTC_NUMERIC &&
             !GDALDataTypeIsComplex(m_oType.GetNumericDataType()))
    {
        const int nDTSize = static_cast<int>(m_oType.GetSize());
        const size_t nElts = abyTile.size() / nDTSize;
        const auto eDT = m_oType.GetNumericDataType();
        return GDALBufferHasOnlyNoData(abyTile.data(), GetNoDataValueAsDouble(),
                                       nElts,        // nWidth
                                       1,            // nHeight
                                       nElts,        // nLineStride
                                       1,            // nComponents
                                       nDTSize * 8,  // nBitsPerSample
                                       GDALDataTypeIsInteger(eDT)
                                           ? (GDALDataTypeIsSigned(eDT)
                                                  ? GSF_SIGNED_INT
                                                  : GSF_UNSIGNED_INT)
                                           : GSF_FLOATING_POINT);
    }
    return false;
}

/************************************************************************/
/*                  ZarrArray::OpenTilePresenceCache()                  */
/************************************************************************/

std::shared_ptr<GDALMDArray>
ZarrArray::OpenTilePresenceCache(bool bCanCreate) const
{
    if (m_bHasTriedCacheTilePresenceArray)
        return m_poCacheTilePresenceArray;
    m_bHasTriedCacheTilePresenceArray = true;

    if (m_nTotalTileCount == 1)
        return nullptr;

    std::string osCacheFilename;
    auto poRGCache = GetCacheRootGroup(bCanCreate, osCacheFilename);
    if (!poRGCache)
        return nullptr;

    const std::string osTilePresenceArrayName(MassageName(GetFullName()) +
                                              "_tile_presence");
    auto poTilePresenceArray = poRGCache->OpenMDArray(osTilePresenceArrayName);
    const auto eByteDT = GDALExtendedDataType::Create(GDT_Byte);
    if (poTilePresenceArray)
    {
        bool ok = true;
        const auto &apoDimsCache = poTilePresenceArray->GetDimensions();
        if (poTilePresenceArray->GetDataType() != eByteDT ||
            apoDimsCache.size() != m_aoDims.size())
        {
            ok = false;
        }
        else
        {
            for (size_t i = 0; i < m_aoDims.size(); i++)
            {
                const auto nExpectedDimSize =
                    (m_aoDims[i]->GetSize() + m_anBlockSize[i] - 1) /
                    m_anBlockSize[i];
                if (apoDimsCache[i]->GetSize() != nExpectedDimSize)
                {
                    ok = false;
                    break;
                }
            }
        }
        if (!ok)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Array %s in %s has not expected characteristics",
                     osTilePresenceArrayName.c_str(), osCacheFilename.c_str());
            return nullptr;
        }

        if (!poTilePresenceArray->GetAttribute("filling_status") && !bCanCreate)
        {
            CPLDebug(ZARR_DEBUG_KEY,
                     "Cache tile presence array for %s found, but filling not "
                     "finished",
                     GetFullName().c_str());
            return nullptr;
        }

        CPLDebug(ZARR_DEBUG_KEY, "Using cache tile presence for %s",
                 GetFullName().c_str());
    }
    else if (bCanCreate)
    {
        int idxDim = 0;
        std::string osBlockSize;
        std::vector<std::shared_ptr<GDALDimension>> apoNewDims;
        for (const auto &poDim : m_aoDims)
        {
            auto poNewDim = poRGCache->CreateDimension(
                osTilePresenceArrayName + '_' + std::to_string(idxDim),
                std::string(), std::string(),
                (poDim->GetSize() + m_anBlockSize[idxDim] - 1) /
                    m_anBlockSize[idxDim]);
            if (!poNewDim)
                return nullptr;
            apoNewDims.emplace_back(poNewDim);

            if (!osBlockSize.empty())
                osBlockSize += ',';
            constexpr GUInt64 BLOCKSIZE = 256;
            osBlockSize +=
                std::to_string(std::min(poNewDim->GetSize(), BLOCKSIZE));

            idxDim++;
        }

        CPLStringList aosOptionsTilePresence;
        aosOptionsTilePresence.SetNameValue("BLOCKSIZE", osBlockSize.c_str());
        poTilePresenceArray =
            poRGCache->CreateMDArray(osTilePresenceArrayName, apoNewDims,
                                     eByteDT, aosOptionsTilePresence.List());
        if (!poTilePresenceArray)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Cannot create %s in %s",
                     osTilePresenceArrayName.c_str(), osCacheFilename.c_str());
            return nullptr;
        }
        poTilePresenceArray->SetNoDataValue(0);
    }
    else
    {
        return nullptr;
    }

    m_poCacheTilePresenceArray = poTilePresenceArray;

    return poTilePresenceArray;
}

/************************************************************************/
/*                    ZarrArray::CacheTilePresence()                    */
/************************************************************************/

bool ZarrArray::CacheTilePresence()
{
    if (m_nTotalTileCount == 1)
        return true;

    const std::string osDirectoryName = GetDataDirectory();

    struct DirCloser
    {
        DirCloser(const DirCloser &) = delete;
        DirCloser &operator=(const DirCloser &) = delete;

        VSIDIR *m_psDir;

        explicit DirCloser(VSIDIR *psDir) : m_psDir(psDir)
        {
        }

        ~DirCloser()
        {
            VSICloseDir(m_psDir);
        }
    };

    auto psDir = VSIOpenDir(osDirectoryName.c_str(), -1, nullptr);
    if (!psDir)
        return false;
    DirCloser dirCloser(psDir);

    auto poTilePresenceArray = OpenTilePresenceCache(true);
    if (!poTilePresenceArray)
    {
        return false;
    }

    if (poTilePresenceArray->GetAttribute("filling_status"))
    {
        CPLDebug(ZARR_DEBUG_KEY,
                 "CacheTilePresence(): %s already filled. Nothing to do",
                 poTilePresenceArray->GetName().c_str());
        return true;
    }

    std::vector<GUInt64> anTileIdx(m_aoDims.size());
    const std::vector<size_t> anCount(m_aoDims.size(), 1);
    const std::vector<GInt64> anArrayStep(m_aoDims.size(), 0);
    const std::vector<GPtrDiff_t> anBufferStride(m_aoDims.size(), 0);
    const auto &apoDimsCache = poTilePresenceArray->GetDimensions();
    const auto eByteDT = GDALExtendedDataType::Create(GDT_Byte);

    CPLDebug(ZARR_DEBUG_KEY,
             "CacheTilePresence(): Iterating over %s to find which tiles are "
             "present...",
             osDirectoryName.c_str());
    uint64_t nCounter = 0;
    const char chSrcFilenameDirSeparator =
        VSIGetDirectorySeparator(osDirectoryName.c_str())[0];
    while (const VSIDIREntry *psEntry = VSIGetNextDirEntry(psDir))
    {
        if (!VSI_ISDIR(psEntry->nMode))
        {
            const CPLStringList aosTokens = GetTileIndicesFromFilename(
                CPLString(psEntry->pszName)
                    .replaceAll(chSrcFilenameDirSeparator, '/')
                    .c_str());
            if (aosTokens.size() == static_cast<int>(m_aoDims.size()))
            {
                // Get tile indices from filename
                bool unexpectedIndex = false;
                for (int i = 0; i < aosTokens.size(); ++i)
                {
                    if (CPLGetValueType(aosTokens[i]) != CPL_VALUE_INTEGER)
                    {
                        unexpectedIndex = true;
                    }
                    anTileIdx[i] =
                        static_cast<GUInt64>(CPLAtoGIntBig(aosTokens[i]));
                    if (anTileIdx[i] >= apoDimsCache[i]->GetSize())
                    {
                        unexpectedIndex = true;
                    }
                }
                if (unexpectedIndex)
                {
                    continue;
                }

                nCounter++;
                if ((nCounter % 1000) == 0)
                {
                    CPLDebug(ZARR_DEBUG_KEY,
                             "CacheTilePresence(): Listing in progress "
                             "(last examined %s, at least %.02f %% completed)",
                             psEntry->pszName,
                             100.0 * double(nCounter) /
                                 double(m_nTotalTileCount));
                }
                constexpr GByte byOne = 1;
                // CPLDebugOnly(ZARR_DEBUG_KEY, "Marking %s has present",
                // psEntry->pszName);
                if (!poTilePresenceArray->Write(
                        anTileIdx.data(), anCount.data(), anArrayStep.data(),
                        anBufferStride.data(), eByteDT, &byOne))
                {
                    return false;
                }
            }
        }
    }
    CPLDebug(ZARR_DEBUG_KEY, "CacheTilePresence(): finished");

    // Write filling_status attribute
    auto poAttr = poTilePresenceArray->CreateAttribute(
        "filling_status", {}, GDALExtendedDataType::CreateString(), nullptr);
    if (poAttr)
    {
        if (nCounter == 0)
            poAttr->Write("no_tile_present");
        else if (nCounter == m_nTotalTileCount)
            poAttr->Write("all_tiles_present");
        else
            poAttr->Write("some_tiles_missing");
    }

    // Force closing
    m_poCacheTilePresenceArray = nullptr;
    m_bHasTriedCacheTilePresenceArray = false;

    return true;
}

/************************************************************************/
/*                      ZarrArray::CreateAttribute()                    */
/************************************************************************/

std::shared_ptr<GDALAttribute> ZarrArray::CreateAttribute(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (!CheckValidAndErrorOutIfNot())
        return nullptr;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return nullptr;
    }
    if (anDimensions.size() >= 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot create attributes of dimension >= 2");
        return nullptr;
    }
    return m_oAttrGroup.CreateAttribute(osName, anDimensions, oDataType,
                                        papszOptions);
}

/************************************************************************/
/*                  ZarrGroupBase::DeleteAttribute()                    */
/************************************************************************/

bool ZarrArray::DeleteAttribute(const std::string &osName, CSLConstList)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    return m_oAttrGroup.DeleteAttribute(osName);
}

/************************************************************************/
/*                      ZarrArray::SetSpatialRef()                      */
/************************************************************************/

bool ZarrArray::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        return GDALPamMDArray::SetSpatialRef(poSRS);
    }
    m_poSRS.reset();
    if (poSRS)
        m_poSRS.reset(poSRS->Clone());
    m_bSRSModified = true;
    return true;
}

/************************************************************************/
/*                         ZarrArray::SetUnit()                         */
/************************************************************************/

bool ZarrArray::SetUnit(const std::string &osUnit)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }
    m_osUnit = osUnit;
    m_bUnitModified = true;
    return true;
}

/************************************************************************/
/*                       ZarrArray::GetOffset()                         */
/************************************************************************/

double ZarrArray::GetOffset(bool *pbHasOffset,
                            GDALDataType *peStorageType) const
{
    if (pbHasOffset)
        *pbHasOffset = m_bHasOffset;
    if (peStorageType)
        *peStorageType = GDT_Unknown;
    return m_dfOffset;
}

/************************************************************************/
/*                       ZarrArray::GetScale()                          */
/************************************************************************/

double ZarrArray::GetScale(bool *pbHasScale, GDALDataType *peStorageType) const
{
    if (pbHasScale)
        *pbHasScale = m_bHasScale;
    if (peStorageType)
        *peStorageType = GDT_Unknown;
    return m_dfScale;
}

/************************************************************************/
/*                       ZarrArray::SetOffset()                         */
/************************************************************************/

bool ZarrArray::SetOffset(double dfOffset, GDALDataType /* eStorageType */)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    m_dfOffset = dfOffset;
    m_bHasOffset = true;
    m_bOffsetModified = true;
    return true;
}

/************************************************************************/
/*                       ZarrArray::SetScale()                          */
/************************************************************************/

bool ZarrArray::SetScale(double dfScale, GDALDataType /* eStorageType */)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    m_dfScale = dfScale;
    m_bHasScale = true;
    m_bScaleModified = true;
    return true;
}

/************************************************************************/
/*                      GetDimensionTypeDirection()                     */
/************************************************************************/

/* static */
void ZarrArray::GetDimensionTypeDirection(CPLJSONObject &oAttributes,
                                          std::string &osType,
                                          std::string &osDirection)
{
    std::string osUnit;
    const auto unit = oAttributes[CF_UNITS];
    if (unit.GetType() == CPLJSONObject::Type::String)
    {
        osUnit = unit.ToString();
    }

    const auto oStdName = oAttributes[CF_STD_NAME];
    if (oStdName.GetType() == CPLJSONObject::Type::String)
    {
        const auto osStdName = oStdName.ToString();
        if (osStdName == CF_PROJ_X_COORD || osStdName == CF_LONGITUDE_STD_NAME)
        {
            osType = GDAL_DIM_TYPE_HORIZONTAL_X;
            oAttributes.Delete(CF_STD_NAME);
            if (osUnit == CF_DEGREES_EAST)
            {
                osDirection = "EAST";
            }
        }
        else if (osStdName == CF_PROJ_Y_COORD ||
                 osStdName == CF_LATITUDE_STD_NAME)
        {
            osType = GDAL_DIM_TYPE_HORIZONTAL_Y;
            oAttributes.Delete(CF_STD_NAME);
            if (osUnit == CF_DEGREES_NORTH)
            {
                osDirection = "NORTH";
            }
        }
        else if (osStdName == "time")
        {
            osType = GDAL_DIM_TYPE_TEMPORAL;
            oAttributes.Delete(CF_STD_NAME);
        }
    }

    const auto osAxis = oAttributes[CF_AXIS].ToString();
    if (osAxis == "Z")
    {
        osType = GDAL_DIM_TYPE_VERTICAL;
        const auto osPositive = oAttributes["positive"].ToString();
        if (osPositive == "up")
        {
            osDirection = "UP";
            oAttributes.Delete("positive");
        }
        else if (osPositive == "down")
        {
            osDirection = "DOWN";
            oAttributes.Delete("positive");
        }
        oAttributes.Delete(CF_AXIS);
    }
}

/************************************************************************/
/*                      GetCoordinateVariables()                        */
/************************************************************************/

std::vector<std::shared_ptr<GDALMDArray>>
ZarrArray::GetCoordinateVariables() const
{
    if (!CheckValidAndErrorOutIfNot())
        return {};

    std::vector<std::shared_ptr<GDALMDArray>> ret;
    const auto poCoordinates = GetAttribute("coordinates");
    if (poCoordinates &&
        poCoordinates->GetDataType().GetClass() == GEDTC_STRING &&
        poCoordinates->GetDimensionCount() == 0)
    {
        const char *pszCoordinates = poCoordinates->ReadAsString();
        if (pszCoordinates)
        {
            auto poGroup = m_poGroupWeak.lock();
            if (!poGroup)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot access coordinate variables of %s has "
                         "belonging group has gone out of scope",
                         GetName().c_str());
            }
            else
            {
                const CPLStringList aosNames(
                    CSLTokenizeString2(pszCoordinates, " ", 0));
                for (int i = 0; i < aosNames.size(); i++)
                {
                    auto poCoordinateVar = poGroup->OpenMDArray(aosNames[i]);
                    if (poCoordinateVar)
                    {
                        ret.emplace_back(poCoordinateVar);
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot find variable corresponding to "
                                 "coordinate %s",
                                 aosNames[i]);
                    }
                }
            }
        }
    }

    return ret;
}

/************************************************************************/
/*                            Resize()                                  */
/************************************************************************/

bool ZarrArray::Resize(const std::vector<GUInt64> &anNewDimSizes,
                       CSLConstList /* papszOptions */)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!IsWritable())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Resize() not supported on read-only file");
        return false;
    }

    const auto nDimCount = GetDimensionCount();
    if (anNewDimSizes.size() != nDimCount)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Not expected number of values in anNewDimSizes.");
        return false;
    }

    auto &dims = GetDimensions();
    std::vector<size_t> anGrownDimIdx;
    std::map<GDALDimension *, GUInt64> oMapDimToSize;
    for (size_t i = 0; i < nDimCount; ++i)
    {
        auto oIter = oMapDimToSize.find(dims[i].get());
        if (oIter != oMapDimToSize.end() && oIter->second != anNewDimSizes[i])
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot resize a dimension referenced several times "
                     "to different sizes");
            return false;
        }
        if (anNewDimSizes[i] != dims[i]->GetSize())
        {
            if (anNewDimSizes[i] < dims[i]->GetSize())
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Resize() does not support shrinking the array.");
                return false;
            }

            oMapDimToSize[dims[i].get()] = anNewDimSizes[i];
            anGrownDimIdx.push_back(i);
        }
        else
        {
            oMapDimToSize[dims[i].get()] = dims[i]->GetSize();
        }
    }
    if (!anGrownDimIdx.empty())
    {
        m_bDefinitionModified = true;
        for (size_t dimIdx : anGrownDimIdx)
        {
            auto dim = std::dynamic_pointer_cast<ZarrDimension>(dims[dimIdx]);
            if (dim)
            {
                dim->SetSize(anNewDimSizes[dimIdx]);
                if (dim->GetName() != dim->GetFullName())
                {
                    // This is not a local dimension
                    m_poSharedResource->UpdateDimensionSize(dim);
                }
            }
            else
            {
                CPLAssert(false);
            }
        }
    }
    return true;
}

/************************************************************************/
/*                       NotifyChildrenOfRenaming()                     */
/************************************************************************/

void ZarrArray::NotifyChildrenOfRenaming()
{
    m_oAttrGroup.ParentRenamed(m_osFullName);
}

/************************************************************************/
/*                          ParentRenamed()                             */
/************************************************************************/

void ZarrArray::ParentRenamed(const std::string &osNewParentFullName)
{
    GDALMDArray::ParentRenamed(osNewParentFullName);

    auto poParent = m_poGroupWeak.lock();
    // The parent necessarily exist, since it notified us
    CPLAssert(poParent);

    m_osFilename =
        CPLFormFilename(CPLFormFilename(poParent->GetDirectoryName().c_str(),
                                        m_osName.c_str(), nullptr),
                        CPLGetFilename(m_osFilename.c_str()), nullptr);
}

/************************************************************************/
/*                              Rename()                                */
/************************************************************************/

bool ZarrArray::Rename(const std::string &osNewName)
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    if (!m_bUpdatable)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }
    if (!ZarrGroupBase::IsValidObjectName(osNewName))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid array name");
        return false;
    }

    auto poParent = m_poGroupWeak.lock();
    if (poParent)
    {
        if (!poParent->CheckArrayOrGroupWithSameNameDoesNotExist(osNewName))
            return false;
    }

    const std::string osRootDirectoryName(
        CPLGetDirname(CPLGetDirname(m_osFilename.c_str())));
    const std::string osOldDirectoryName =
        CPLFormFilename(osRootDirectoryName.c_str(), m_osName.c_str(), nullptr);
    const std::string osNewDirectoryName = CPLFormFilename(
        osRootDirectoryName.c_str(), osNewName.c_str(), nullptr);

    if (VSIRename(osOldDirectoryName.c_str(), osNewDirectoryName.c_str()) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Renaming of %s to %s failed",
                 osOldDirectoryName.c_str(), osNewDirectoryName.c_str());
        return false;
    }

    m_poSharedResource->RenameZMetadataRecursive(osOldDirectoryName,
                                                 osNewDirectoryName);

    m_osFilename =
        CPLFormFilename(osNewDirectoryName.c_str(),
                        CPLGetFilename(m_osFilename.c_str()), nullptr);

    if (poParent)
    {
        poParent->NotifyArrayRenamed(m_osName, osNewName);
    }

    BaseRename(osNewName);

    return true;
}

/************************************************************************/
/*                       NotifyChildrenOfDeletion()                     */
/************************************************************************/

void ZarrArray::NotifyChildrenOfDeletion()
{
    m_oAttrGroup.ParentDeleted();
}

/************************************************************************/
/*                     ParseSpecialAttributes()                         */
/************************************************************************/

void ZarrArray::ParseSpecialAttributes(
    const std::shared_ptr<GDALGroup> &poGroup, CPLJSONObject &oAttributes)
{
    const auto crs = oAttributes[CRS_ATTRIBUTE_NAME];
    std::shared_ptr<OGRSpatialReference> poSRS;
    if (crs.GetType() == CPLJSONObject::Type::Object)
    {
        for (const char *key : {"url", "wkt", "projjson"})
        {
            const auto item = crs[key];
            if (item.IsValid())
            {
                poSRS = std::make_shared<OGRSpatialReference>();
                if (poSRS->SetFromUserInput(
                        item.ToString().c_str(),
                        OGRSpatialReference::
                            SET_FROM_USER_INPUT_LIMITATIONS_get()) ==
                    OGRERR_NONE)
                {
                    oAttributes.Delete(CRS_ATTRIBUTE_NAME);
                    break;
                }
                poSRS.reset();
            }
        }
    }
    else
    {
        // Check if SRS is using CF-1 conventions
        const auto gridMapping = oAttributes["grid_mapping"];
        if (gridMapping.GetType() == CPLJSONObject::Type::String)
        {
            const auto gridMappingArray =
                poGroup->OpenMDArray(gridMapping.ToString());
            if (gridMappingArray)
            {
                poSRS = std::make_shared<OGRSpatialReference>();
                CPLStringList aosKeyValues;
                for (const auto &poAttr : gridMappingArray->GetAttributes())
                {
                    if (poAttr->GetDataType().GetClass() == GEDTC_STRING)
                    {
                        aosKeyValues.SetNameValue(poAttr->GetName().c_str(),
                                                  poAttr->ReadAsString());
                    }
                    else if (poAttr->GetDataType().GetClass() == GEDTC_NUMERIC)
                    {
                        std::string osVal;
                        for (double val : poAttr->ReadAsDoubleArray())
                        {
                            if (!osVal.empty())
                                osVal += ',';
                            osVal += CPLSPrintf("%.18g", val);
                        }
                        aosKeyValues.SetNameValue(poAttr->GetName().c_str(),
                                                  osVal.c_str());
                    }
                }
                if (poSRS->importFromCF1(aosKeyValues.List(), nullptr) !=
                    OGRERR_NONE)
                {
                    poSRS.reset();
                }
            }
        }
    }

    if (poSRS)
    {
        int iDimX = 0;
        int iDimY = 0;
        int iCount = 1;
        for (const auto &poDim : GetDimensions())
        {
            if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X)
                iDimX = iCount;
            else if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y)
                iDimY = iCount;
            iCount++;
        }
        if ((iDimX == 0 || iDimY == 0) && GetDimensionCount() >= 2)
        {
            iDimX = static_cast<int>(GetDimensionCount());
            iDimY = iDimX - 1;
        }
        if (iDimX > 0 && iDimY > 0)
        {
            if (poSRS->GetDataAxisToSRSAxisMapping() == std::vector<int>{2, 1})
                poSRS->SetDataAxisToSRSAxisMapping({iDimY, iDimX});
            else if (poSRS->GetDataAxisToSRSAxisMapping() ==
                     std::vector<int>{1, 2})
                poSRS->SetDataAxisToSRSAxisMapping({iDimX, iDimY});
        }

        SetSRS(poSRS);
    }

    const auto unit = oAttributes[CF_UNITS];
    if (unit.GetType() == CPLJSONObject::Type::String)
    {
        std::string osUnit = unit.ToString();
        oAttributes.Delete(CF_UNITS);
        RegisterUnit(osUnit);
    }

    const auto offset = oAttributes[CF_ADD_OFFSET];
    const auto offsetType = offset.GetType();
    if (offsetType == CPLJSONObject::Type::Integer ||
        offsetType == CPLJSONObject::Type::Long ||
        offsetType == CPLJSONObject::Type::Double)
    {
        double dfOffset = offset.ToDouble();
        oAttributes.Delete(CF_ADD_OFFSET);
        RegisterOffset(dfOffset);
    }

    const auto scale = oAttributes[CF_SCALE_FACTOR];
    const auto scaleType = scale.GetType();
    if (scaleType == CPLJSONObject::Type::Integer ||
        scaleType == CPLJSONObject::Type::Long ||
        scaleType == CPLJSONObject::Type::Double)
    {
        double dfScale = scale.ToDouble();
        oAttributes.Delete(CF_SCALE_FACTOR);
        RegisterScale(dfScale);
    }
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

bool ZarrArray::SetStatistics(bool bApproxStats, double dfMin, double dfMax,
                              double dfMean, double dfStdDev,
                              GUInt64 nValidCount, CSLConstList papszOptions)
{
    if (!bApproxStats && m_bUpdatable &&
        CPLTestBool(
            CSLFetchNameValueDef(papszOptions, "UPDATE_METADATA", "NO")))
    {
        auto poAttr = GetAttribute("actual_range");
        if (!poAttr)
        {
            poAttr =
                CreateAttribute("actual_range", {2}, GetDataType(), nullptr);
        }
        if (poAttr)
        {
            std::vector<GUInt64> startIdx = {0};
            std::vector<size_t> count = {2};
            std::vector<double> values = {dfMin, dfMax};
            poAttr->Write(startIdx.data(), count.data(), nullptr, nullptr,
                          GDALExtendedDataType::Create(GDT_Float64),
                          values.data(), nullptr, 0);
        }
    }
    return GDALPamMDArray::SetStatistics(bApproxStats, dfMin, dfMax, dfMean,
                                         dfStdDev, nValidCount, papszOptions);
}
