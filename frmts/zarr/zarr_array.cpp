/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr.h"

#include "cpl_float.h"
#include "cpl_mem_cache.h"
#include "cpl_multiproc.h"
#include "cpl_vsi_virtual.h"
#include "ucs4_utf8.hpp"
#include "gdal_thread_pool.h"

#include "netcdf_cf_constants.h"  // for CF_UNITS, etc

#include <algorithm>
#include <cassert>
#include <cmath>
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
/*                     ZarrArray::ParseChunkSize()                      */
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
/*                    ZarrArray::ComputeBlockCount()                    */
/************************************************************************/

/* static */ uint64_t ZarrArray::ComputeBlockCount(
    const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const std::vector<GUInt64> &anBlockSize)
{
    uint64_t nTotalBlockCount = 1;
    for (size_t i = 0; i < aoDims.size(); ++i)
    {
        const uint64_t nBlockThisDim =
            cpl::div_round_up(aoDims[i]->GetSize(), anBlockSize[i]);
        if (nBlockThisDim != 0 &&
            nTotalBlockCount >
                std::numeric_limits<uint64_t>::max() / nBlockThisDim)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Array %s has more than 2^64 blocks. This is not supported.",
                osName.c_str());
            return 0;
        }
        nTotalBlockCount *= nBlockThisDim;
    }
    return nTotalBlockCount;
}

/************************************************************************/
/*                   ComputeCountInnerBlockInOuter()                    */
/************************************************************************/

static std::vector<GUInt64>
ComputeCountInnerBlockInOuter(const std::vector<GUInt64> &anInnerBlockSize,
                              const std::vector<GUInt64> &anOuterBlockSize)
{
    std::vector<GUInt64> ret;
    CPLAssert(anInnerBlockSize.size() == anOuterBlockSize.size());
    for (size_t i = 0; i < anInnerBlockSize.size(); ++i)
    {
        // All those assertions must be checked by the caller before
        // constructing the ZarrArray instance.
        CPLAssert(anInnerBlockSize[i] > 0);
        CPLAssert(anInnerBlockSize[i] <= anOuterBlockSize[i]);
        CPLAssert((anOuterBlockSize[i] % anInnerBlockSize[i]) == 0);
        ret.push_back(anOuterBlockSize[i] / anInnerBlockSize[i]);
    }
    return ret;
}

/************************************************************************/
/*                     ComputeInnerBlockSizeBytes()                     */
/************************************************************************/

static size_t
ComputeInnerBlockSizeBytes(const std::vector<DtypeElt> &aoDtypeElts,
                           const std::vector<GUInt64> &anInnerBlockSize)
{
    const size_t nSourceSize =
        aoDtypeElts.back().nativeOffset + aoDtypeElts.back().nativeSize;
    size_t nInnerBlockSizeBytes = nSourceSize;
    for (const auto &nBlockSize : anInnerBlockSize)
    {
        // Given that ParseChunkSize() has checked that the outer block size
        // fits on size_t, and that m_anInnerBlockSize[i] <= m_anOuterBlockSize[i],
        // this cast is safe, and the multiplication cannot overflow.
        nInnerBlockSizeBytes *= static_cast<size_t>(nBlockSize);
    }
    return nInnerBlockSizeBytes;
}

/************************************************************************/
/*                        ZarrArray::ZarrArray()                        */
/************************************************************************/

ZarrArray::ZarrArray(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const std::shared_ptr<ZarrGroupBase> &poParent, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::vector<DtypeElt> &aoDtypeElts,
    const std::vector<GUInt64> &anOuterBlockSize,
    const std::vector<GUInt64> &anInnerBlockSize)
    :
#if !defined(COMPILER_WARNS_ABOUT_ABSTRACT_VBASE_INIT)
      GDALAbstractMDArray(poParent->GetFullName(), osName),
#endif
      GDALPamMDArray(poParent->GetFullName(), osName,
                     poSharedResource->GetPAM()),
      m_poSharedResource(poSharedResource), m_poParent(poParent),
      m_aoDims(aoDims), m_oType(oType), m_aoDtypeElts(aoDtypeElts),
      m_anOuterBlockSize(anOuterBlockSize),
      m_anInnerBlockSize(anInnerBlockSize),
      m_anCountInnerBlockInOuter(ComputeCountInnerBlockInOuter(
          m_anInnerBlockSize, m_anOuterBlockSize)),
      m_nTotalInnerChunkCount(
          ComputeBlockCount(osName, aoDims, m_anInnerBlockSize)),
      m_nInnerBlockSizeBytes(
          m_nTotalInnerChunkCount > 0
              ? ComputeInnerBlockSizeBytes(m_aoDtypeElts, m_anInnerBlockSize)
              : 0),
      m_oAttrGroup(m_osFullName, /*bContainerIsGroup=*/false),
      m_bUseOptimizedCodePaths(CPLTestBool(
          CPLGetConfigOption("GDAL_ZARR_USE_OPTIMIZED_CODE_PATHS", "YES")))
{
}

/************************************************************************/
/*                             ~ZarrArray()                             */
/************************************************************************/

ZarrArray::~ZarrArray()
{
    if (m_pabyNoData)
    {
        m_oType.FreeDynamicMemory(&m_pabyNoData[0]);
        CPLFree(m_pabyNoData);
    }

    DeallocateDecodedBlockData();
}

/************************************************************************/
/*               ZarrArray::SerializeSpecialAttributes()                */
/************************************************************************/

CPLJSONObject ZarrArray::SerializeSpecialAttributes()
{
    m_bSRSModified = false;
    m_oAttrGroup.UnsetModified();

    auto oAttrs = m_oAttrGroup.Serialize();

    const bool bUseSpatialProjConventions =
        EQUAL(m_aosCreationOptions.FetchNameValueDef(
                  "GEOREFERENCING_CONVENTION", "GDAL"),
              "SPATIAL_PROJ");

    const auto ExportToWkt2AndPROJJSON = [this](CPLJSONObject &oContainer,
                                                const char *pszWKT2AttrName,
                                                const char *pszPROJJSONAttrName)
    {
        const char *const apszOptions[] = {"FORMAT=WKT2_2019", nullptr};
        char *pszWKT = nullptr;
        if (m_poSRS->exportToWkt(&pszWKT, apszOptions) == OGRERR_NONE)
        {
            oContainer.Set(pszWKT2AttrName, pszWKT);
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
                    oContainer.Set(pszPROJJSONAttrName, oDocProjJSON.GetRoot());
                }
            }
            CPLFree(projjson);
        }
    };

    CPLJSONArray oZarrConventionsArray;
    if (bUseSpatialProjConventions)
    {
        if (m_poSRS)
        {
            CPLJSONObject oConventionProj;
            oConventionProj.Set(
                "schema_url",
                "https://raw.githubusercontent.com/zarr-experimental/geo-proj/"
                "refs/tags/v1/schema.json");
            oConventionProj.Set("spec_url",
                                "https://github.com/zarr-experimental/geo-proj/"
                                "blob/v1/README.md");
            oConventionProj.Set("uuid", "f17cb550-5864-4468-aeb7-f3180cfb622f");
            oConventionProj.Set("name", "proj:");  // ending colon intended
            oConventionProj.Set(
                "description",
                "Coordinate reference system information for geospatial data");

            oZarrConventionsArray.Add(oConventionProj);

            const char *pszAuthorityName = m_poSRS->GetAuthorityName(nullptr);
            const char *pszAuthorityCode = m_poSRS->GetAuthorityCode(nullptr);
            if (pszAuthorityName && pszAuthorityCode)
            {
                oAttrs.Set("proj:code", CPLSPrintf("%s:%s", pszAuthorityName,
                                                   pszAuthorityCode));
            }
            else
            {
                ExportToWkt2AndPROJJSON(oAttrs, "proj:wkt2", "proj:projjson");
            }
        }

        if (GetDimensionCount() >= 2)
        {
            bool bAddSpatialProjConvention = false;

            double dfXOff = std::numeric_limits<double>::quiet_NaN();
            double dfXRes = std::numeric_limits<double>::quiet_NaN();
            double dfYOff = std::numeric_limits<double>::quiet_NaN();
            double dfYRes = std::numeric_limits<double>::quiet_NaN();
            std::string osDimXName;
            std::string osDimYName;
            std::string osDimZName;
            double dfWidth = 0, dfHeight = 0;
            for (const auto &poDim : GetDimensions())
            {
                if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X)
                {
                    osDimXName = poDim->GetName();
                    dfWidth = static_cast<double>(poDim->GetSize());
                    auto poVar = poDim->GetIndexingVariable();
                    if (poVar && poVar->IsRegularlySpaced(dfXOff, dfXRes))
                    {
                        dfXOff -= dfXRes / 2;
                    }
                }
                else if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y)
                {
                    osDimYName = poDim->GetName();
                    dfHeight = static_cast<double>(poDim->GetSize());
                    auto poVar = poDim->GetIndexingVariable();
                    if (poVar && poVar->IsRegularlySpaced(dfYOff, dfYRes))
                    {
                        dfYOff -= dfYRes / 2;
                    }
                }
                else if (poDim->GetType() == GDAL_DIM_TYPE_VERTICAL)
                {
                    osDimZName = poDim->GetName();
                }
            }

            GDALGeoTransform gt;
            if (!osDimXName.empty() && !osDimYName.empty())
            {
                const auto oGDALGeoTransform = oAttrs["gdal:geotransform"];
                const bool bHasGDALGeoTransform =
                    (oGDALGeoTransform.GetType() ==
                         CPLJSONObject::Type::Array &&
                     oGDALGeoTransform.ToArray().size() == 6);
                if (bHasGDALGeoTransform)
                {
                    const auto oGDALGeoTransformArray =
                        oGDALGeoTransform.ToArray();
                    for (int i = 0; i < 6; ++i)
                    {
                        gt[i] = oGDALGeoTransformArray[i].ToDouble();
                    }
                    bAddSpatialProjConvention = true;
                }
                else if (!std::isnan(dfXOff) && !std::isnan(dfXRes) &&
                         !std::isnan(dfYOff) && !std::isnan(dfYRes))
                {
                    gt.xorig = dfXOff;
                    gt.xscale = dfXRes;
                    gt.xrot = 0;  // xrot
                    gt.yorig = dfYOff;
                    gt.yrot = 0;  // yrot
                    gt.yscale = dfYRes;
                    bAddSpatialProjConvention = true;
                }
            }

            if (bAddSpatialProjConvention)
            {
                const auto osGDALMD_AREA_OR_POINT =
                    oAttrs.GetString(GDALMD_AREA_OR_POINT);
                if (osGDALMD_AREA_OR_POINT == GDALMD_AOP_AREA)
                {
                    oAttrs.Add("spatial:registration", "pixel");
                    oAttrs.Delete(GDALMD_AREA_OR_POINT);
                }
                else if (osGDALMD_AREA_OR_POINT == GDALMD_AOP_POINT)
                {
                    oAttrs.Add("spatial:registration", "node");
                    oAttrs.Delete(GDALMD_AREA_OR_POINT);

                    // Going from GDAL's corner convention to pixel center
                    gt.xorig += 0.5 * gt.xscale + 0.5 * gt.xrot;
                    gt.yorig += 0.5 * gt.yrot + 0.5 * gt.yscale;
                    dfWidth -= 1.0;
                    dfHeight -= 1.0;
                }

                CPLJSONArray oAttrSpatialTransform;
                oAttrSpatialTransform.Add(gt.xscale);  // xres
                oAttrSpatialTransform.Add(gt.xrot);    // xrot
                oAttrSpatialTransform.Add(gt.xorig);   // xoff
                oAttrSpatialTransform.Add(gt.yrot);    // yrot
                oAttrSpatialTransform.Add(gt.yscale);  // yres
                oAttrSpatialTransform.Add(gt.yorig);   // yoff

                oAttrs.Add("spatial:transform_type", "affine");
                oAttrs.Add("spatial:transform", oAttrSpatialTransform);
                oAttrs.Delete("gdal:geotransform");

                double dfX0, dfY0;
                double dfX1, dfY1;
                double dfX2, dfY2;
                double dfX3, dfY3;
                gt.Apply(0, 0, &dfX0, &dfY0);
                gt.Apply(dfWidth, 0, &dfX1, &dfY1);
                gt.Apply(0, dfHeight, &dfX2, &dfY2);
                gt.Apply(dfWidth, dfHeight, &dfX3, &dfY3);
                const double dfXMin =
                    std::min(std::min(dfX0, dfX1), std::min(dfX2, dfX3));
                const double dfYMin =
                    std::min(std::min(dfY0, dfY1), std::min(dfY2, dfY3));
                const double dfXMax =
                    std::max(std::max(dfX0, dfX1), std::max(dfX2, dfX3));
                const double dfYMax =
                    std::max(std::max(dfY0, dfY1), std::max(dfY2, dfY3));

                CPLJSONArray oAttrSpatialBBOX;
                oAttrSpatialBBOX.Add(dfXMin);
                oAttrSpatialBBOX.Add(dfYMin);
                oAttrSpatialBBOX.Add(dfXMax);
                oAttrSpatialBBOX.Add(dfYMax);
                oAttrs.Add("spatial:bbox", oAttrSpatialBBOX);

                CPLJSONArray aoSpatialDimensions;
                if (!osDimZName.empty())
                    aoSpatialDimensions.Add(osDimZName);
                aoSpatialDimensions.Add(osDimYName);
                aoSpatialDimensions.Add(osDimXName);
                oAttrs.Add("spatial:dimensions", aoSpatialDimensions);

                CPLJSONObject oConventionSpatial;
                oConventionSpatial.Set(
                    "schema_url",
                    "https://raw.githubusercontent.com/zarr-conventions/"
                    "spatial/refs/tags/v1/schema.json");
                oConventionSpatial.Set("spec_url",
                                       "https://github.com/zarr-conventions/"
                                       "spatial/blob/v1/README.md");
                oConventionSpatial.Set("uuid",
                                       "689b58e2-cf7b-45e0-9fff-9cfc0883d6b4");
                oConventionSpatial.Set("name",
                                       "spatial:");  // ending colon intended
                oConventionSpatial.Set("description",
                                       "Spatial coordinate information");

                oZarrConventionsArray.Add(oConventionSpatial);
            }
        }

        if (oZarrConventionsArray.size() > 0)
        {
            oAttrs.Add("zarr_conventions", oZarrConventionsArray);
        }
    }
    else if (m_poSRS)
    {
        // GDAL convention

        CPLJSONObject oCRS;
        ExportToWkt2AndPROJJSON(oCRS, "wkt", "projjson");

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
/*                           FillBlockSize()                            */
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
            const auto v = static_cast<GUInt64>(CPLAtoGIntBig(aszTokens[i]));
            if (v > 0)
            {
                anBlockSize[i] = v;
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
/*                     DeallocateDecodedBlockData()                     */
/************************************************************************/

void ZarrArray::DeallocateDecodedBlockData()
{
    if (!m_abyDecodedBlockData.empty())
    {
        const size_t nDTSize = m_oType.GetSize();
        GByte *pDst = &m_abyDecodedBlockData[0];
        const size_t nValues = m_abyDecodedBlockData.size() / nDTSize;
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
/*                 ZarrArray::SerializeNumericNoData()                  */
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
/*                      ZarrArray::GetSpatialRef()                      */
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
/*                         SetRawNoDataValue()                          */
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
/*                          DecodeSourceElt()                           */
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
/*                    ZarrArray::IAdviseReadCommon()                    */
/************************************************************************/

bool ZarrArray::IAdviseReadCommon(const GUInt64 *arrayStartIdx,
                                  const size_t *count,
                                  CSLConstList papszOptions,
                                  std::vector<uint64_t> &anIndicesCur,
                                  int &nThreadsMax,
                                  std::vector<uint64_t> &anReqBlocksIndices,
                                  size_t &nReqBlocks) const
{
    if (!CheckValidAndErrorOutIfNot())
        return false;

    const size_t nDims = m_aoDims.size();
    anIndicesCur.resize(nDims);
    std::vector<uint64_t> anIndicesMin(nDims);
    std::vector<uint64_t> anIndicesMax(nDims);

    // Compute min and max tile indices in each dimension, and the total
    // number of tiles this represents.
    nReqBlocks = 1;
    for (size_t i = 0; i < nDims; ++i)
    {
        anIndicesMin[i] = arrayStartIdx[i] / m_anInnerBlockSize[i];
        anIndicesMax[i] =
            (arrayStartIdx[i] + count[i] - 1) / m_anInnerBlockSize[i];
        // Overflow on number of tiles already checked in Create()
        nReqBlocks *=
            static_cast<size_t>(anIndicesMax[i] - anIndicesMin[i] + 1);
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
    // Also check that anReqBlocksIndices size computation won't overflow.
    if (nReqBlocks > nCacheSize / std::max(m_nInnerBlockSizeBytes, nDims))
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "CACHE_SIZE=" CPL_FRMT_GUIB " is not big enough to cache "
                 "all needed tiles. "
                 "At least " CPL_FRMT_GUIB " bytes would be needed",
                 static_cast<GUIntBig>(nCacheSize),
                 static_cast<GUIntBig>(
                     nReqBlocks * std::max(m_nInnerBlockSizeBytes, nDims)));
        return false;
    }

    nThreadsMax = GDALGetNumThreads(papszOptions, "NUM_THREADS",
                                    GDAL_DEFAULT_MAX_THREAD_COUNT,
                                    /* bDefaultAllCPUs=*/true);
    if (nThreadsMax <= 1)
        return true;
    CPLDebug(ZARR_DEBUG_KEY, "IAdviseRead(): Using up to %d threads",
             nThreadsMax);

    m_oChunkCache.clear();

    // Overflow checked above
    try
    {
        anReqBlocksIndices.resize(nDims * nReqBlocks);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate anReqBlocksIndices: %s", e.what());
        return false;
    }

    size_t dimIdx = 0;
    size_t nBlockIter = 0;
lbl_next_depth:
    if (dimIdx == nDims)
    {
        if (nDims == 2)
        {
            // optimize in common case
            memcpy(&anReqBlocksIndices[nBlockIter * nDims], anIndicesCur.data(),
                   sizeof(uint64_t) * 2);
        }
        else if (nDims == 3)
        {
            // optimize in common case
            memcpy(&anReqBlocksIndices[nBlockIter * nDims], anIndicesCur.data(),
                   sizeof(uint64_t) * 3);
        }
        else
        {
            memcpy(&anReqBlocksIndices[nBlockIter * nDims], anIndicesCur.data(),
                   sizeof(uint64_t) * nDims);
        }
        nBlockIter++;
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
    assert(nBlockIter == nReqBlocks);

    return true;
}

/************************************************************************/
/*                          ZarrArray::IRead()                          */
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

    std::vector<uint64_t> blockIndices(nDims);
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
        bool bEmptyChunk = false;

        const GByte *pabySrcBlock = m_abyDecodedBlockData.empty()
                                        ? m_abyRawBlockData.data()
                                        : m_abyDecodedBlockData.data();
        bool bMatchFoundInMapChunkIndexToCachedBlock = false;

        // Use cache built by IAdviseRead() if possible
        if (!m_oChunkCache.empty())
        {
            const auto oIter = m_oChunkCache.find(blockIndices);
            if (oIter != m_oChunkCache.end())
            {
                bMatchFoundInMapChunkIndexToCachedBlock = true;
                if (oIter->second.abyDecoded.empty())
                {
                    bEmptyChunk = true;
                }
                else
                {
                    pabySrcBlock = oIter->second.abyDecoded.data();
                }
            }
            else
            {
#ifdef DEBUG
                std::string key;
                for (size_t j = 0; j < nDims; ++j)
                {
                    if (j)
                        key += ',';
                    key += std::to_string(blockIndices[j]);
                }
                CPLDebugOnly(ZARR_DEBUG_KEY, "Cache miss for tile %s",
                             key.c_str());
#endif
            }
        }

        if (!bMatchFoundInMapChunkIndexToCachedBlock)
        {
            if (!blockIndices.empty() && blockIndices == m_anCachedBlockIndices)
            {
                if (!m_bCachedBlockValid)
                    return false;
                bEmptyChunk = m_bCachedBlockEmpty;
            }
            else
            {
                if (!FlushDirtyBlock())
                    return false;

                m_anCachedBlockIndices = blockIndices;
                m_bCachedBlockValid =
                    LoadBlockData(blockIndices.data(), bEmptyChunk);
                if (!m_bCachedBlockValid)
                {
                    return false;
                }
                m_bCachedBlockEmpty = bEmptyChunk;
            }

            pabySrcBlock = m_abyDecodedBlockData.empty()
                               ? m_abyRawBlockData.data()
                               : m_abyDecodedBlockData.data();
        }
        const size_t nSrcDTSize =
            m_abyDecodedBlockData.empty() ? nSourceSize : nDTSize;

        for (size_t i = 0; i < nDims; ++i)
        {
            countInnerLoopInit[i] = 1;
            if (arrayStep[i] != 0)
            {
                const auto nextBlockIdx =
                    std::min((1 + indicesOuterLoop[i] / m_anInnerBlockSize[i]) *
                                 m_anInnerBlockSize[i],
                             arrayStartIdx[i] + count[i] * arrayStep[i]);
                countInnerLoopInit[i] = static_cast<size_t>(cpl::div_round_up(
                    nextBlockIdx - indicesOuterLoop[i], arrayStep[i]));
            }
        }

        if (bEmptyChunk && bBothAreNumericDT && abyTargetNoData.empty())
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
                GDALCopyWords(&zero, GDT_UInt8, 0, &abyTargetNoData[0],
                              bufferDataType.GetNumericDataType(), 0, 1);
            }
        }

    lbl_next_depth_inner_loop:
        if (nDims == 0 || dimIdxSubLoop == nDims - 1)
        {
            indicesInnerLoop[dimIdxSubLoop] = indicesOuterLoop[dimIdxSubLoop];
            void *dst_ptr = dstPtrStackInnerLoop[dimIdxSubLoop];

            if (m_bUseOptimizedCodePaths && bEmptyChunk && bBothAreNumericDT &&
                bNoDataIsZero &&
                nBufferDTSize == dstBufferStrideBytes[dimIdxSubLoop])
            {
                memset(dst_ptr, 0,
                       nBufferDTSize * countInnerLoopInit[dimIdxSubLoop]);
                goto end_inner_loop;
            }
            else if (m_bUseOptimizedCodePaths && bEmptyChunk &&
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
            else if (bEmptyChunk)
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
                    nOffset * m_anInnerBlockSize[i] +
                    (indicesInnerLoop[i] -
                     blockIndices[i] * m_anInnerBlockSize[i]));
            }
            const GByte *src_ptr = pabySrcBlock + nOffset * nSrcDTSize;
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
        blockIndices[dimIdx] =
            indicesOuterLoop[dimIdx] / m_anInnerBlockSize[dimIdx];
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
            if (static_cast<GUInt64>(arrayStep[dimIdx]) <
                m_anInnerBlockSize[dimIdx])
            {
                // Compute index at next block boundary
                auto newIdx =
                    indicesOuterLoop[dimIdx] +
                    (m_anInnerBlockSize[dimIdx] -
                     (indicesOuterLoop[dimIdx] % m_anInnerBlockSize[dimIdx]));
                // And round up compared to arrayStartIdx, arrayStep
                nIncr = static_cast<size_t>(cpl::div_round_up(
                    newIdx - indicesOuterLoop[dimIdx], arrayStep[dimIdx]));
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
            blockIndices[dimIdx] =
                indicesOuterLoop[dimIdx] / m_anInnerBlockSize[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                         ZarrArray::IWrite()                          */
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

    m_oChunkCache.clear();

    // Need to be kept in top-level scope
    std::vector<GUInt64> arrayStartIdxMod;
    std::vector<GInt64> arrayStepMod;
    std::vector<GPtrDiff_t> bufferStrideMod;

    const size_t nDims = m_aoDims.size();
    bool negativeStep = false;
    bool bWriteWholeBlockInit = true;
    for (size_t i = 0; i < nDims; ++i)
    {
        if (arrayStep[i] < 0)
        {
            negativeStep = true;
            if (arrayStep[i] != -1 && count[i] > 1)
                bWriteWholeBlockInit = false;
        }
        else if (arrayStep[i] != 1 && count[i] > 1)
            bWriteWholeBlockInit = false;
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

    std::vector<uint64_t> blockIndices(nDims);
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
        bool bWriteWholeBlock = bWriteWholeBlockInit;
        bool bPartialBlock = false;
        for (size_t i = 0; i < nDims; ++i)
        {
            countInnerLoopInit[i] = 1;
            if (arrayStep[i] != 0)
            {
                const auto nextBlockIdx =
                    std::min((1 + indicesOuterLoop[i] / m_anOuterBlockSize[i]) *
                                 m_anOuterBlockSize[i],
                             arrayStartIdx[i] + count[i] * arrayStep[i]);
                countInnerLoopInit[i] = static_cast<size_t>(cpl::div_round_up(
                    nextBlockIdx - indicesOuterLoop[i], arrayStep[i]));
            }
            if (bWriteWholeBlock)
            {
                const bool bWholePartialBlockThisDim =
                    indicesOuterLoop[i] == 0 &&
                    countInnerLoopInit[i] == m_aoDims[i]->GetSize();
                bWriteWholeBlock =
                    (countInnerLoopInit[i] == m_anOuterBlockSize[i] ||
                     bWholePartialBlockThisDim);
                if (bWholePartialBlockThisDim)
                {
                    bPartialBlock = true;
                }
            }
        }

        size_t dimIdxSubLoop = 0;
        srcPtrStackInnerLoop[0] = srcPtrStackOuterLoop[nDims];
        const size_t nCacheDTSize =
            m_abyDecodedBlockData.empty() ? nNativeSize : nDTSize;
        auto &abyBlock = m_abyDecodedBlockData.empty() ? m_abyRawBlockData
                                                       : m_abyDecodedBlockData;

        if (!blockIndices.empty() && blockIndices == m_anCachedBlockIndices)
        {
            if (!m_bCachedBlockValid)
                return false;
        }
        else
        {
            if (!FlushDirtyBlock())
                return false;

            m_anCachedBlockIndices = blockIndices;
            m_bCachedBlockValid = true;

            if (bWriteWholeBlock)
            {
                if (bPartialBlock)
                {
                    DeallocateDecodedBlockData();
                    memset(&abyBlock[0], 0, abyBlock.size());
                }
            }
            else
            {
                // If we don't write the whole tile, we need to fetch a
                // potentially existing one.
                bool bEmptyBlock = false;
                m_bCachedBlockValid =
                    LoadBlockData(blockIndices.data(), bEmptyBlock);
                if (!m_bCachedBlockValid)
                {
                    return false;
                }

                if (bEmptyBlock)
                {
                    DeallocateDecodedBlockData();

                    if (m_pabyNoData == nullptr)
                    {
                        memset(&abyBlock[0], 0, abyBlock.size());
                    }
                    else
                    {
                        const size_t nElts = abyBlock.size() / nCacheDTSize;
                        GByte *dstPtr = &abyBlock[0];
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
        m_bDirtyBlock = true;
        m_bCachedBlockEmpty = false;
        if (nDims)
            offsetDstBuffer[0] = static_cast<size_t>(
                indicesOuterLoop[0] - blockIndices[0] * m_anOuterBlockSize[0]);

        GByte *pabyBlock = &abyBlock[0];

    lbl_next_depth_inner_loop:
        if (dimIdxSubLoop == dimIdxForCopy)
        {
            size_t nOffset = offsetDstBuffer[dimIdxSubLoop];
            GInt64 step = nDims == 0 ? 0 : arrayStep[dimIdxSubLoop];
            for (size_t i = dimIdxSubLoop + 1; i < nDims; ++i)
            {
                nOffset = static_cast<size_t>(
                    nOffset * m_anOuterBlockSize[i] +
                    (indicesOuterLoop[i] -
                     blockIndices[i] * m_anOuterBlockSize[i]));
                step *= m_anOuterBlockSize[i];
            }
            const void *src_ptr = srcPtrStackInnerLoop[dimIdxSubLoop];
            GByte *dst_ptr = pabyBlock + nOffset * nCacheDTSize;

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
                offsetDstBuffer[dimIdxSubLoop] = static_cast<size_t>(
                    offsetDstBuffer[dimIdxSubLoop - 1] *
                        m_anOuterBlockSize[dimIdxSubLoop] +
                    (indicesOuterLoop[dimIdxSubLoop] -
                     blockIndices[dimIdxSubLoop] *
                         m_anOuterBlockSize[dimIdxSubLoop]));
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
        blockIndices[dimIdx] =
            indicesOuterLoop[dimIdx] / m_anOuterBlockSize[dimIdx];
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
            if (static_cast<GUInt64>(arrayStep[dimIdx]) <
                m_anOuterBlockSize[dimIdx])
            {
                // Compute index at next block boundary
                auto newIdx =
                    indicesOuterLoop[dimIdx] +
                    (m_anOuterBlockSize[dimIdx] -
                     (indicesOuterLoop[dimIdx] % m_anOuterBlockSize[dimIdx]));
                // And round up compared to arrayStartIdx, arrayStep
                nIncr = static_cast<size_t>(cpl::div_round_up(
                    newIdx - indicesOuterLoop[dimIdx], arrayStep[dimIdx]));
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
            blockIndices[dimIdx] =
                indicesOuterLoop[dimIdx] / m_anOuterBlockSize[dimIdx];
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                      ZarrArray::IsEmptyBlock()                       */
/************************************************************************/

bool ZarrArray::IsEmptyBlock(const ZarrByteVectorQuickResize &abyBlock) const
{
    if (m_pabyNoData == nullptr || (m_oType.GetClass() == GEDTC_NUMERIC &&
                                    GetNoDataValueAsDouble() == 0.0))
    {
        const size_t nBytes = abyBlock.size();
        size_t i = 0;
        for (; i + (sizeof(size_t) - 1) < nBytes; i += sizeof(size_t))
        {
            if (*reinterpret_cast<const size_t *>(abyBlock.data() + i) != 0)
            {
                return false;
            }
        }
        for (; i < nBytes; ++i)
        {
            if (abyBlock[i] != 0)
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
        const size_t nElts = abyBlock.size() / nDTSize;
        const auto eDT = m_oType.GetNumericDataType();
        return GDALBufferHasOnlyNoData(
            abyBlock.data(), GetNoDataValueAsDouble(),
            nElts,        // nWidth
            1,            // nHeight
            nElts,        // nLineStride
            1,            // nComponents
            nDTSize * 8,  // nBitsPerSample
            GDALDataTypeIsInteger(eDT)
                ? (GDALDataTypeIsSigned(eDT) ? GSF_SIGNED_INT
                                             : GSF_UNSIGNED_INT)
                : GSF_FLOATING_POINT);
    }
    return false;
}

/************************************************************************/
/*                 ZarrArray::OpenBlockPresenceCache()                  */
/************************************************************************/

std::shared_ptr<GDALMDArray>
ZarrArray::OpenBlockPresenceCache(bool bCanCreate) const
{
    if (m_bHasTriedBlockCachePresenceArray)
        return m_poBlockCachePresenceArray;
    m_bHasTriedBlockCachePresenceArray = true;

    if (m_nTotalInnerChunkCount == 1)
        return nullptr;

    std::string osCacheFilename;
    auto poRGCache = GetCacheRootGroup(bCanCreate, osCacheFilename);
    if (!poRGCache)
        return nullptr;

    const std::string osBlockPresenceArrayName(MassageName(GetFullName()) +
                                               "_tile_presence");
    auto poBlockPresenceArray =
        poRGCache->OpenMDArray(osBlockPresenceArrayName);
    const auto eByteDT = GDALExtendedDataType::Create(GDT_UInt8);
    if (poBlockPresenceArray)
    {
        bool ok = true;
        const auto &apoDimsCache = poBlockPresenceArray->GetDimensions();
        if (poBlockPresenceArray->GetDataType() != eByteDT ||
            apoDimsCache.size() != m_aoDims.size())
        {
            ok = false;
        }
        else
        {
            for (size_t i = 0; i < m_aoDims.size(); i++)
            {
                const auto nExpectedDimSize = cpl::div_round_up(
                    m_aoDims[i]->GetSize(), m_anInnerBlockSize[i]);
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
                     osBlockPresenceArrayName.c_str(), osCacheFilename.c_str());
            return nullptr;
        }

        if (!poBlockPresenceArray->GetAttribute("filling_status") &&
            !bCanCreate)
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
                osBlockPresenceArrayName + '_' + std::to_string(idxDim),
                std::string(), std::string(),
                cpl::div_round_up(poDim->GetSize(),
                                  m_anInnerBlockSize[idxDim]));
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

        CPLStringList aosOptionsBlockPresence;
        aosOptionsBlockPresence.SetNameValue("BLOCKSIZE", osBlockSize.c_str());
        poBlockPresenceArray =
            poRGCache->CreateMDArray(osBlockPresenceArrayName, apoNewDims,
                                     eByteDT, aosOptionsBlockPresence.List());
        if (!poBlockPresenceArray)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Cannot create %s in %s",
                     osBlockPresenceArrayName.c_str(), osCacheFilename.c_str());
            return nullptr;
        }
        poBlockPresenceArray->SetNoDataValue(0);
    }
    else
    {
        return nullptr;
    }

    m_poBlockCachePresenceArray = poBlockPresenceArray;

    return poBlockPresenceArray;
}

/************************************************************************/
/*                   ZarrArray::BlockCachePresence()                    */
/************************************************************************/

bool ZarrArray::BlockCachePresence()
{
    if (m_nTotalInnerChunkCount == 1)
        return true;

    const std::string osDirectoryName = GetDataDirectory();

    auto psDir = std::unique_ptr<VSIDIR, decltype(&VSICloseDir)>(
        VSIOpenDir(osDirectoryName.c_str(), -1, nullptr), VSICloseDir);
    if (!psDir)
        return false;

    auto poBlockPresenceArray = OpenBlockPresenceCache(true);
    if (!poBlockPresenceArray)
    {
        return false;
    }

    if (poBlockPresenceArray->GetAttribute("filling_status"))
    {
        CPLDebug(ZARR_DEBUG_KEY,
                 "BlockCachePresence(): %s already filled. Nothing to do",
                 poBlockPresenceArray->GetName().c_str());
        return true;
    }

    const auto nDims = m_aoDims.size();
    std::vector<GUInt64> anInnerBlockIdx(nDims);
    std::vector<GUInt64> anInnerBlockCounter(nDims);
    const std::vector<size_t> anCount(nDims, 1);
    const std::vector<GInt64> anArrayStep(nDims, 0);
    const std::vector<GPtrDiff_t> anBufferStride(nDims, 0);
    const auto &apoDimsCache = poBlockPresenceArray->GetDimensions();
    const auto eByteDT = GDALExtendedDataType::Create(GDT_UInt8);

    CPLDebug(ZARR_DEBUG_KEY,
             "BlockCachePresence(): Iterating over %s to find which tiles are "
             "present...",
             osDirectoryName.c_str());
    uint64_t nCounter = 0;
    const char chSrcFilenameDirSeparator =
        VSIGetDirectorySeparator(osDirectoryName.c_str())[0];
    while (const VSIDIREntry *psEntry = VSIGetNextDirEntry(psDir.get()))
    {
        if (!VSI_ISDIR(psEntry->nMode))
        {
            const CPLStringList aosTokens = GetChunkIndicesFromFilename(
                CPLString(psEntry->pszName)
                    .replaceAll(chSrcFilenameDirSeparator, '/')
                    .c_str());
            if (aosTokens.size() == static_cast<int>(nDims))
            {
                // Get tile indices from filename
                bool unexpectedIndex = false;
                uint64_t nInnerChunksInOuter = 1;
                for (int i = 0; i < aosTokens.size(); ++i)
                {
                    if (CPLGetValueType(aosTokens[i]) != CPL_VALUE_INTEGER)
                    {
                        unexpectedIndex = true;
                    }
                    anInnerBlockIdx[i] =
                        static_cast<GUInt64>(CPLAtoGIntBig(aosTokens[i]));
                    const auto nInnerChunkCounterThisDim =
                        m_anCountInnerBlockInOuter[i];
                    nInnerChunksInOuter *= nInnerChunkCounterThisDim;
                    if (anInnerBlockIdx[i] >=
                        apoDimsCache[i]->GetSize() / nInnerChunkCounterThisDim)
                    {
                        unexpectedIndex = true;
                    }
                    anInnerBlockIdx[i] *= nInnerChunkCounterThisDim;
                }
                if (unexpectedIndex)
                {
                    continue;
                }

                std::fill(anInnerBlockCounter.begin(),
                          anInnerBlockCounter.end(), 0);

                for (uint64_t iInnerChunk = 0;
                     iInnerChunk < nInnerChunksInOuter; ++iInnerChunk)
                {
                    if (iInnerChunk > 0)
                    {
                        // Update chunk coordinates
                        size_t iDim = m_anInnerBlockSize.size() - 1;
                        const auto nInnerChunkCounterThisDim =
                            m_anCountInnerBlockInOuter[iDim];

                        ++anInnerBlockIdx[iDim];
                        ++anInnerBlockCounter[iDim];

                        while (anInnerBlockCounter[iDim] ==
                               nInnerChunkCounterThisDim)
                        {
                            anInnerBlockIdx[iDim] -= nInnerChunkCounterThisDim;
                            anInnerBlockCounter[iDim] = 0;
                            --iDim;

                            ++anInnerBlockIdx[iDim];
                            ++anInnerBlockCounter[iDim];
                        }
                    }

                    nCounter++;
                    if ((nCounter % 1000) == 0)
                    {
                        CPLDebug(
                            ZARR_DEBUG_KEY,
                            "BlockCachePresence(): Listing in progress "
                            "(last examined %s, at least %.02f %% completed)",
                            psEntry->pszName,
                            100.0 * double(nCounter) /
                                double(m_nTotalInnerChunkCount));
                    }
                    constexpr GByte byOne = 1;
                    // CPLDebugOnly(ZARR_DEBUG_KEY, "Marking %s has present",
                    // psEntry->pszName);
                    if (!poBlockPresenceArray->Write(
                            anInnerBlockIdx.data(), anCount.data(),
                            anArrayStep.data(), anBufferStride.data(), eByteDT,
                            &byOne))
                    {
                        return false;
                    }
                }
            }
        }
    }
    CPLDebug(ZARR_DEBUG_KEY, "BlockCachePresence(): finished");

    // Write filling_status attribute
    auto poAttr = poBlockPresenceArray->CreateAttribute(
        "filling_status", {}, GDALExtendedDataType::CreateString(), nullptr);
    if (poAttr)
    {
        if (nCounter == 0)
            poAttr->Write("no_tile_present");
        else if (nCounter == m_nTotalInnerChunkCount)
            poAttr->Write("all_tiles_present");
        else
            poAttr->Write("some_tiles_missing");
    }

    // Force closing
    m_poBlockCachePresenceArray = nullptr;
    m_bHasTriedBlockCachePresenceArray = false;

    return true;
}

/************************************************************************/
/*                     ZarrArray::CreateAttribute()                     */
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
/*                   ZarrGroupBase::DeleteAttribute()                   */
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
/*                        ZarrArray::GetOffset()                        */
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
/*                        ZarrArray::GetScale()                         */
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
/*                        ZarrArray::SetOffset()                        */
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
/*                        ZarrArray::SetScale()                         */
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
/*                     GetDimensionTypeDirection()                      */
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
/*                       GetCoordinateVariables()                       */
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
/*                               Resize()                               */
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
/*                      NotifyChildrenOfRenaming()                      */
/************************************************************************/

void ZarrArray::NotifyChildrenOfRenaming()
{
    m_oAttrGroup.ParentRenamed(m_osFullName);
}

/************************************************************************/
/*                           ParentRenamed()                            */
/************************************************************************/

void ZarrArray::ParentRenamed(const std::string &osNewParentFullName)
{
    GDALMDArray::ParentRenamed(osNewParentFullName);

    auto poParent = m_poGroupWeak.lock();
    // The parent necessarily exist, since it notified us
    CPLAssert(poParent);

    m_osFilename = CPLFormFilenameSafe(
        CPLFormFilenameSafe(poParent->GetDirectoryName().c_str(),
                            m_osName.c_str(), nullptr)
            .c_str(),
        CPLGetFilename(m_osFilename.c_str()), nullptr);
}

/************************************************************************/
/*                               Rename()                               */
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
        CPLGetDirnameSafe(CPLGetDirnameSafe(m_osFilename.c_str()).c_str()));
    const std::string osOldDirectoryName = CPLFormFilenameSafe(
        osRootDirectoryName.c_str(), m_osName.c_str(), nullptr);
    const std::string osNewDirectoryName = CPLFormFilenameSafe(
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
        CPLFormFilenameSafe(osNewDirectoryName.c_str(),
                            CPLGetFilename(m_osFilename.c_str()), nullptr);

    if (poParent)
    {
        poParent->NotifyArrayRenamed(m_osName, osNewName);
    }

    BaseRename(osNewName);

    return true;
}

/************************************************************************/
/*                      NotifyChildrenOfDeletion()                      */
/************************************************************************/

void ZarrArray::NotifyChildrenOfDeletion()
{
    m_oAttrGroup.ParentDeleted();
}

/************************************************************************/
/*                            ParseProjCRS()                            */
/************************************************************************/

static void ParseProjCRS(const ZarrAttributeGroup *poAttrGroup,
                         CPLJSONObject &oAttributes, bool bFoundProjUUID,
                         std::shared_ptr<OGRSpatialReference> &poSRS)
{
    const auto poAttrProjCode =
        bFoundProjUUID ? poAttrGroup->GetAttribute("proj:code") : nullptr;
    const char *pszProjCode =
        poAttrProjCode ? poAttrProjCode->ReadAsString() : nullptr;
    if (pszProjCode)
    {
        poSRS = std::make_shared<OGRSpatialReference>();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRS->SetFromUserInput(
                pszProjCode,
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
            OGRERR_NONE)
        {
            poSRS.reset();
        }
        else
        {
            oAttributes.Delete("proj:code");
        }
    }
    else
    {
        // EOP Sentinel Zarr Samples Service only
        const auto poAttrProjEPSG = poAttrGroup->GetAttribute("proj:epsg");
        if (poAttrProjEPSG)
        {
            poSRS = std::make_shared<OGRSpatialReference>();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (poSRS->importFromEPSG(poAttrProjEPSG->ReadAsInt()) !=
                OGRERR_NONE)
            {
                poSRS.reset();
            }
            else
            {
                oAttributes.Delete("proj:epsg");
            }
        }
        else
        {
            // Both EOPF Sentinel Zarr Samples Service and geo-proj convention
            const auto poAttrProjWKT2 = poAttrGroup->GetAttribute("proj:wkt2");
            const char *pszProjWKT2 =
                poAttrProjWKT2 ? poAttrProjWKT2->ReadAsString() : nullptr;
            if (pszProjWKT2)
            {
                poSRS = std::make_shared<OGRSpatialReference>();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (poSRS->importFromWkt(pszProjWKT2) != OGRERR_NONE)
                {
                    poSRS.reset();
                }
                else
                {
                    oAttributes.Delete("proj:wkt2");
                }
            }
            else if (bFoundProjUUID)
            {
                // geo-proj convention
                const auto poAttrProjPROJJSON =
                    poAttrGroup->GetAttribute("proj:projjson");
                const char *pszProjPROJJSON =
                    poAttrProjPROJJSON ? poAttrProjPROJJSON->ReadAsString()
                                       : nullptr;
                if (pszProjPROJJSON)
                {
                    poSRS = std::make_shared<OGRSpatialReference>();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    if (poSRS->SetFromUserInput(
                            pszProjPROJJSON,
                            OGRSpatialReference::
                                SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
                        OGRERR_NONE)
                    {
                        poSRS.reset();
                    }
                    else
                    {
                        oAttributes.Delete("proj:projjson");
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                      ParseSpatialConventions()                       */
/************************************************************************/

static void ParseSpatialConventions(
    const std::shared_ptr<ZarrSharedResource> &poSharedResource,
    const ZarrAttributeGroup *poAttrGroup, CPLJSONObject &oAttributes,
    std::shared_ptr<OGRSpatialReference> &poSRS, bool &bAxisAssigned,
    const std::vector<std::shared_ptr<GDALDimension>> &apoDims)
{
    // From https://github.com/zarr-conventions/spatial
    const auto poAttrSpatialDimensions =
        poAttrGroup->GetAttribute("spatial:dimensions");
    if (!poAttrSpatialDimensions)
        return;

    const auto aosSpatialDimensions =
        poAttrSpatialDimensions->ReadAsStringArray();
    if (aosSpatialDimensions.size() < 2)
        return;

    int iDimNameY = 0;
    int iDimNameX = 0;
    const char *pszNameY =
        aosSpatialDimensions[aosSpatialDimensions.size() - 2];
    const char *pszNameX =
        aosSpatialDimensions[aosSpatialDimensions.size() - 1];
    int iDim = 1;
    for (const auto &poDim : apoDims)
    {
        if (poDim->GetName() == pszNameX)
            iDimNameX = iDim;
        else if (poDim->GetName() == pszNameY)
            iDimNameY = iDim;
        ++iDim;
    }
    if (iDimNameX == 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "spatial:dimensions[%d] = %s is a unknown "
                 "Zarr dimension",
                 static_cast<int>(aosSpatialDimensions.size() - 1), pszNameX);
    }
    if (iDimNameY == 0)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "spatial_dimensions[%d] = %s is a unknown "
                 "Zarr dimension",
                 static_cast<int>(aosSpatialDimensions.size() - 2), pszNameY);
    }

    if (iDimNameX > 0 && iDimNameY > 0)
    {
        oAttributes.Delete("spatial:dimensions");

        if (!bAxisAssigned && poSRS)
        {
            const auto &oMapping = poSRS->GetDataAxisToSRSAxisMapping();
            if (oMapping == std::vector<int>{2, 1} ||
                oMapping == std::vector<int>{2, 1, 3})
                poSRS->SetDataAxisToSRSAxisMapping({iDimNameY, iDimNameX});
            else if (oMapping == std::vector<int>{1, 2} ||
                     oMapping == std::vector<int>{1, 2, 3})
                poSRS->SetDataAxisToSRSAxisMapping({iDimNameX, iDimNameY});

            bAxisAssigned = true;
        }
    }

    const auto poAttrSpatialRegistration =
        poAttrGroup->GetAttribute("spatial:registration");
    bool bIsNodeRegistration = false;
    if (!poAttrSpatialRegistration)
        oAttributes.Set("spatial:registration", "pixel");  // default value
    else
    {
        const char *pszSpatialRegistration =
            poAttrSpatialRegistration->ReadAsString();
        if (pszSpatialRegistration &&
            strcmp(pszSpatialRegistration, "node") == 0)
            bIsNodeRegistration = true;
    }

    const auto poAttrSpatialTransform =
        poAttrGroup->GetAttribute("spatial:transform");
    const auto poAttrSpatialTransformType =
        poAttrGroup->GetAttribute("spatial:transform_type");
    const char *pszAttrSpatialTransformType =
        poAttrSpatialTransformType ? poAttrSpatialTransformType->ReadAsString()
                                   : nullptr;

    if (poAttrSpatialTransform &&
        (!pszAttrSpatialTransformType ||
         strcmp(pszAttrSpatialTransformType, "affine") == 0))
    {
        auto adfSpatialTransform = poAttrSpatialTransform->ReadAsDoubleArray();
        if (adfSpatialTransform.size() == 6)
        {
            oAttributes.Delete("spatial:transform");
            oAttributes.Delete("spatial:transform_type");

            // If we have rotation/shear coefficients, expose a gdal:geotransform
            // attributes
            if (adfSpatialTransform[1] != 0 || adfSpatialTransform[3] != 0)
            {
                if (bIsNodeRegistration)
                {
                    // From pixel center convention to GDAL's corner convention
                    adfSpatialTransform[2] -= 0.5 * adfSpatialTransform[0] +
                                              0.5 * adfSpatialTransform[1];
                    adfSpatialTransform[5] -= 0.5 * adfSpatialTransform[3] +
                                              0.5 * adfSpatialTransform[4];
                }

                CPLJSONArray oGeoTransform;
                // Reorder coefficients to GDAL convention
                for (int idx : {2, 0, 1, 5, 3, 4})
                    oGeoTransform.Add(adfSpatialTransform[idx]);
                oAttributes["gdal:geotransform"] = oGeoTransform;
            }
            else
            {
                auto &poDimX = apoDims[iDimNameX - 1];
                auto &poDimY = apoDims[iDimNameY - 1];
                if (!dynamic_cast<GDALMDArrayRegularlySpaced *>(
                        poDimX->GetIndexingVariable().get()) &&
                    !dynamic_cast<GDALMDArrayRegularlySpaced *>(
                        poDimY->GetIndexingVariable().get()))
                {
                    auto poIndexingVarX = GDALMDArrayRegularlySpaced::Create(
                        std::string(), poDimX->GetName(), poDimX,
                        adfSpatialTransform[2] + adfSpatialTransform[0] / 2,
                        adfSpatialTransform[0], 0);
                    poDimX->SetIndexingVariable(poIndexingVarX);

                    // Make the shared resource hold a strong
                    // reference on the indexing variable,
                    // so that it remains available to anyone
                    // querying the dimension for it.
                    poSharedResource->RegisterIndexingVariable(
                        poDimX->GetFullName(), poIndexingVarX);

                    auto poIndexingVarY = GDALMDArrayRegularlySpaced::Create(
                        std::string(), poDimY->GetName(), poDimY,
                        adfSpatialTransform[5] + adfSpatialTransform[4] / 2,
                        adfSpatialTransform[4], 0);
                    poDimY->SetIndexingVariable(poIndexingVarY);
                    poSharedResource->RegisterIndexingVariable(
                        poDimY->GetFullName(), poIndexingVarY);
                }
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "spatial:transform[] contains an "
                     "unexpected number of values: %d",
                     static_cast<int>(adfSpatialTransform.size()));
        }
    }
}

/************************************************************************/
/*               DetectSRSFromEOPFSampleServiceMetadata()               */
/************************************************************************/

/* This function is derived from ExtractCoordinateMetadata() of
 * https://github.com/EOPF-Sample-Service/GDAL-ZARR-EOPF/blob/main/src/eopf_metadata.cpp
 * released under the MIT license and
 * Copyright (c) 2024 Yuvraj Adagale and contributors
 *
 * Note: it does not handle defaulting to EPSG:4326 as it is not clear to me
 * (E. Rouault) why this would be needed, at least for Sentinel2 L1C or L2
 * products
 */
static void DetectSRSFromEOPFSampleServiceMetadata(
    const std::string &osRootDirectoryName,
    const std::shared_ptr<ZarrGroupBase> &poGroup,
    std::shared_ptr<OGRSpatialReference> &poSRS)
{
    const CPLJSONObject obj = poGroup->GetAttributeGroup().Serialize();

    // -----------------------------------
    // STEP 1: Extract spatial reference information
    // -----------------------------------

    // Find EPSG code directly or in STAC properties
    int nEPSGCode = 0;
    const CPLJSONObject &stacDiscovery = obj.GetObj("stac_discovery");
    if (stacDiscovery.IsValid())
    {
        const CPLJSONObject &properties = stacDiscovery.GetObj("properties");
        if (properties.IsValid())
        {
            // Try to get proj:epsg as a number first, then as a string
            nEPSGCode = properties.GetInteger("proj:epsg", 0);
            if (nEPSGCode <= 0)
            {
                nEPSGCode =
                    std::atoi(properties.GetString("proj:epsg", "").c_str());
            }
            if (nEPSGCode > 0)
            {
                CPLDebugOnly(ZARR_DEBUG_KEY,
                             "Found proj:epsg in STAC properties: %d",
                             nEPSGCode);
            }
        }
    }

    // If not found in STAC, try top level
    if (nEPSGCode <= 0)
    {
        nEPSGCode = obj.GetInteger("proj:epsg", obj.GetInteger("epsg", 0));
        if (nEPSGCode <= 0)
        {
            nEPSGCode = std::atoi(
                obj.GetString("proj:epsg", obj.GetString("epsg", "")).c_str());
        }
        if (nEPSGCode > 0)
        {
            CPLDebugOnly(ZARR_DEBUG_KEY, "Found proj:epsg at top level: %d",
                         nEPSGCode);
        }
    }

    // If still not found, simple search in common locations
    if (nEPSGCode <= 0)
    {
        for (const auto &child : obj.GetChildren())
        {
            if (child.GetType() == CPLJSONObject::Type::Object)
            {
                nEPSGCode =
                    child.GetInteger("proj:epsg", child.GetInteger("epsg", 0));
                if (nEPSGCode <= 0)
                {
                    nEPSGCode = std::atoi(
                        child
                            .GetString("proj:epsg", child.GetString("epsg", ""))
                            .c_str());
                }
                if (nEPSGCode > 0)
                {
                    CPLDebugOnly(ZARR_DEBUG_KEY,
                                 "Found proj:epsg in child %s: %d",
                                 child.GetName().c_str(), nEPSGCode);
                    break;
                }
            }
        }
    }

    // Enhanced search for STAC discovery metadata with better structure parsing
    if (nEPSGCode <= 0 && stacDiscovery.IsValid())
    {
        // Try to get the full STAC item
        const CPLJSONObject &geometry = stacDiscovery.GetObj("geometry");

        if (geometry.IsValid())
        {
            const CPLJSONObject &geomCrs = geometry.GetObj("crs");
            if (geomCrs.IsValid())
            {
                const CPLJSONObject &geomProps = geomCrs.GetObj("properties");
                if (geomProps.IsValid())
                {
                    const int nGeomEpsg = geomProps.GetInteger("code", 0);
                    if (nGeomEpsg != 0)
                    {
                        nEPSGCode = nGeomEpsg;
                        CPLDebugOnly(ZARR_DEBUG_KEY,
                                     "Found CRS code in STAC geometry: %d",
                                     nEPSGCode);
                    }
                }
            }
        }
    }

    // Try to infer CRS from Sentinel-2 tile naming convention
    if (nEPSGCode <= 0)
    {
        // Look for Sentinel-2 tile ID pattern in dataset name or metadata
        std::string tileName;

        // First, try to extract from dataset name if it contains T##XXX pattern
        std::string dsNameStr(CPLGetFilename(osRootDirectoryName.c_str()));
        const size_t tilePos = dsNameStr.find("_T");
        if (tilePos != std::string::npos && tilePos + 6 < dsNameStr.length())
        {
            tileName = dsNameStr.substr(tilePos + 1, 6);  // Extract T##XXX
            CPLDebugOnly(ZARR_DEBUG_KEY,
                         "Extracted tile name from dataset name: %s",
                         tileName.c_str());
        }

        // Also check in STAC discovery metadata
        if (tileName.empty() && stacDiscovery.IsValid())
        {
            const CPLJSONObject &properties =
                stacDiscovery.GetObj("properties");
            if (properties.IsValid())
            {
                tileName = properties.GetString(
                    "s2:mgrs_tile",
                    properties.GetString("mgrs_tile",
                                         properties.GetString("tile_id", "")));
                if (!tileName.empty())
                {
                    CPLDebug("EOPFZARR",
                             "Found tile name in STAC properties: %s",
                             tileName.c_str());
                }
            }
        }

        // Parse tile name to get EPSG code (T##XXX -> UTM Zone ## North/South)
        if (!tileName.empty() && tileName.length() >= 3 && tileName[0] == 'T')
        {
            // Extract zone number (characters 1-2)
            const std::string zoneStr = tileName.substr(1, 2);
            const int zone = std::atoi(zoneStr.c_str());

            if (zone >= 1 && zone <= 60)
            {
                // Determine hemisphere from the third character
                const char hemisphere =
                    tileName.length() > 3 ? tileName[3] : 'N';

                // For Sentinel-2, assume Northern hemisphere unless explicitly Southern
                // Most Sentinel-2 data is Northern hemisphere
                // Cf https://en.wikipedia.org/wiki/Military_Grid_Reference_System#Grid_zone_designation
                const bool isNorth = (hemisphere >= 'N' && hemisphere <= 'X');

                nEPSGCode = isNorth ? (32600 + zone) : (32700 + zone);
                CPLDebugOnly(ZARR_DEBUG_KEY,
                             "Inferred EPSG %d from Sentinel-2 tile %s (zone "
                             "%d, %s hemisphere)",
                             nEPSGCode, tileName.c_str(), zone,
                             isNorth ? "North" : "South");
            }
        }
    }

    if (nEPSGCode > 0)
    {
        poSRS = std::make_shared<OGRSpatialReference>();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRS->importFromEPSG(nEPSGCode) == OGRERR_NONE)
        {
            return;
        }
        poSRS.reset();
    }

    // Look for WKT
    std::string wkt = obj.GetString("spatial_ref", "");
    if (wkt.empty() && stacDiscovery.IsValid())
    {
        const CPLJSONObject &properties = stacDiscovery.GetObj("properties");
        if (properties.IsValid())
        {
            wkt = properties.GetString("spatial_ref", "");
        }
    }
    if (!wkt.empty())
    {
        poSRS = std::make_shared<OGRSpatialReference>();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRS->importFromWkt(wkt.c_str()) == OGRERR_NONE)
        {
            return;
        }
        poSRS.reset();
    }
}

/************************************************************************/
/*                           SetAttributes()                            */
/************************************************************************/

void ZarrArray::SetAttributes(const std::shared_ptr<ZarrGroupBase> &poGroup,
                              CPLJSONObject &oAttributes)
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
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
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
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
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
                            osVal += CPLSPrintf("%.17g", val);
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

    // For EOPF Sentinel Zarr Samples Service datasets, read attributes from
    // the STAC Proj extension attributes to get the CRS.
    // There is also partly intersection with https://github.com/zarr-conventions/geo-proj
    // For zarr-conventions/geo-proj and zarr-conventions/spatial, try first
    // at array level, then parent and finally grandparent.

    auto poRootGroup = std::dynamic_pointer_cast<ZarrGroupBase>(GetRootGroup());

    std::vector<const ZarrAttributeGroup *> apoAttrGroup;

    ZarrAttributeGroup oThisAttrGroup(std::string(),
                                      /* bContainerIsGroup = */ false);
    oThisAttrGroup.Init(oAttributes, /* bUpdatable=*/false);

    apoAttrGroup.push_back(&oThisAttrGroup);
    if (GetDimensionCount() >= 2)
    {
        apoAttrGroup.push_back(&(poGroup->GetAttributeGroup()));
        // Only use root group to detect conventions
        if (poRootGroup)
            apoAttrGroup.push_back(&(poRootGroup->GetAttributeGroup()));
    }

    // Look for declaration of geo-proj and spatial conventions
    bool bFoundSpatialUUID = false;
    bool bFoundProjUUID = false;
    for (const ZarrAttributeGroup *poAttrGroup : apoAttrGroup)
    {
        const auto poAttrZarrConventions =
            poAttrGroup->GetAttribute("zarr_conventions");
        if (poAttrZarrConventions)
        {
            const char *pszZarrConventions =
                poAttrZarrConventions->ReadAsString();
            if (pszZarrConventions)
            {
                CPLJSONDocument oDoc;
                if (oDoc.LoadMemory(pszZarrConventions))
                {
                    const auto oZarrConventions = oDoc.GetRoot();
                    if (oZarrConventions.GetType() ==
                        CPLJSONObject::Type::Array)
                    {
                        const auto oZarrConventionsArray =
                            oZarrConventions.ToArray();

                        const auto hasSpatialUUIDLambda =
                            [](const CPLJSONObject &obj)
                        {
                            constexpr const char *SPATIAL_UUID =
                                "689b58e2-cf7b-45e0-9fff-9cfc0883d6b4";
                            return obj.GetString("uuid") == SPATIAL_UUID;
                        };
                        bFoundSpatialUUID =
                            std::find_if(oZarrConventionsArray.begin(),
                                         oZarrConventionsArray.end(),
                                         hasSpatialUUIDLambda) !=
                            oZarrConventionsArray.end();

                        const auto hasProjUUIDLambda =
                            [](const CPLJSONObject &obj)
                        {
                            constexpr const char *PROJ_UUID =
                                "f17cb550-5864-4468-aeb7-f3180cfb622f";
                            return obj.GetString("uuid") == PROJ_UUID;
                        };
                        bFoundProjUUID =
                            std::find_if(oZarrConventionsArray.begin(),
                                         oZarrConventionsArray.end(),
                                         hasProjUUIDLambda) !=
                            oZarrConventionsArray.end();
                    }
                }
            }
            break;
        }
    }

    // If there is neither spatial nor geo-proj, just consider the current array
    // for EOPF Sentinel Zarr Samples Service datasets
    if (!bFoundSpatialUUID && !bFoundProjUUID)
        apoAttrGroup.resize(1);
    else if (apoAttrGroup.size() == 3)
        apoAttrGroup.resize(2);

    bool bAxisAssigned = false;
    for (const ZarrAttributeGroup *poAttrGroup : apoAttrGroup)
    {
        if (!poSRS)
        {
            ParseProjCRS(poAttrGroup, oAttributes, bFoundProjUUID, poSRS);
        }

        if (GetDimensionCount() >= 2 && bFoundSpatialUUID)
        {
            const bool bAxisAssignedBefore = bAxisAssigned;
            ParseSpatialConventions(m_poSharedResource, poAttrGroup,
                                    oAttributes, poSRS, bAxisAssigned,
                                    m_aoDims);
            if (bAxisAssigned && !bAxisAssignedBefore)
                SetSRS(poSRS);

            // Note: we ignore EOPF Sentinel Zarr Samples Service "proj:transform"
            // attribute, as we don't need to
            // use it since the x and y dimensions are already associated with a
            // 1-dimensional array with the values.
        }
    }

    if (!poSRS && poRootGroup && oAttributes.GetObj("_eopf_attrs").IsValid())
    {
        DetectSRSFromEOPFSampleServiceMetadata(
            m_poSharedResource->GetRootDirectoryName(), poRootGroup, poSRS);
    }

    if (poSRS && !bAxisAssigned)
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
            const auto &oMapping = poSRS->GetDataAxisToSRSAxisMapping();
            if (oMapping == std::vector<int>{2, 1} ||
                oMapping == std::vector<int>{2, 1, 3})
                poSRS->SetDataAxisToSRSAxisMapping({iDimY, iDimX});
            else if (oMapping == std::vector<int>{1, 2} ||
                     oMapping == std::vector<int>{1, 2, 3})
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

    m_oAttrGroup.Init(oAttributes, m_bUpdatable);
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

/************************************************************************/
/*               ZarrArray::IsBlockMissingFromCacheInfo()               */
/************************************************************************/

bool ZarrArray::IsBlockMissingFromCacheInfo(const std::string &osFilename,
                                            const uint64_t *blockIndices) const
{
    CPL_IGNORE_RET_VAL(osFilename);
    auto poBlockPresenceArray = OpenBlockPresenceCache(false);
    if (poBlockPresenceArray)
    {
        std::vector<GUInt64> anBlockIdx(m_aoDims.size());
        const std::vector<size_t> anCount(m_aoDims.size(), 1);
        const std::vector<GInt64> anArrayStep(m_aoDims.size(), 0);
        const std::vector<GPtrDiff_t> anBufferStride(m_aoDims.size(), 0);
        const auto eByteDT = GDALExtendedDataType::Create(GDT_UInt8);
        for (size_t i = 0; i < m_aoDims.size(); ++i)
        {
            anBlockIdx[i] = static_cast<GUInt64>(blockIndices[i]);
        }
        GByte byValue = 0;
        if (poBlockPresenceArray->Read(
                anBlockIdx.data(), anCount.data(), anArrayStep.data(),
                anBufferStride.data(), eByteDT, &byValue) &&
            byValue == 0)
        {
            CPLDebugOnly(ZARR_DEBUG_KEY, "Block %s missing (=nodata)",
                         osFilename.c_str());
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                     ZarrArray::GetRawBlockInfo()                     */
/************************************************************************/

bool ZarrArray::GetRawBlockInfo(const uint64_t *panBlockCoordinates,
                                GDALMDArrayRawBlockInfo &info) const
{
    info.clear();
    for (size_t i = 0; i < m_anInnerBlockSize.size(); ++i)
    {
        const auto nBlockSize = m_anInnerBlockSize[i];
        const auto nBlockCount =
            cpl::div_round_up(m_aoDims[i]->GetSize(), nBlockSize);
        if (panBlockCoordinates[i] >= nBlockCount)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GetRawBlockInfo() failed: array %s: "
                     "invalid block coordinate (%u) for dimension %u",
                     GetName().c_str(),
                     static_cast<unsigned>(panBlockCoordinates[i]),
                     static_cast<unsigned>(i));
            return false;
        }
    }

    std::vector<uint64_t> anOuterBlockIndices;
    for (size_t i = 0; i < m_anCountInnerBlockInOuter.size(); ++i)
    {
        anOuterBlockIndices.push_back(panBlockCoordinates[i] /
                                      m_anCountInnerBlockInOuter[i]);
    }

    std::string osFilename = BuildChunkFilename(anOuterBlockIndices.data());

    // For network file systems, get the streaming version of the filename,
    // as we don't need arbitrary seeking in the file
    osFilename = VSIFileManager::GetHandler(osFilename.c_str())
                     ->GetStreamingFilename(osFilename);
    {
        std::lock_guard<std::mutex> oLock(m_oMutex);
        if (IsBlockMissingFromCacheInfo(osFilename, panBlockCoordinates))
            return true;
    }

    VSILFILE *fp = nullptr;
    // This is the number of files returned in a S3 directory listing operation
    const char *const apszOpenOptions[] = {"IGNORE_FILENAME_RESTRICTIONS=YES",
                                           nullptr};
    const auto nErrorBefore = CPLGetErrorCounter();
    {
        // Avoid issuing ReadDir() when a lot of files are expected
        CPLConfigOptionSetter optionSetter("GDAL_DISABLE_READDIR_ON_OPEN",
                                           "YES", true);
        fp = VSIFOpenEx2L(osFilename.c_str(), "rb", 0, apszOpenOptions);
    }
    if (fp == nullptr)
    {
        if (nErrorBefore != CPLGetErrorCounter())
        {
            return false;
        }
        else
        {
            // Missing files are OK and indicate nodata_value
            return true;
        }
    }
    VSIFSeekL(fp, 0, SEEK_END);
    const auto nFileSize = VSIFTellL(fp);
    VSIFCloseL(fp);

    // For Kerchunk files, get information on the actual location
    const CPLStringList aosMetadata(
        VSIGetFileMetadata(osFilename.c_str(), "CHUNK_INFO", nullptr));
    if (!aosMetadata.empty())
    {
        const char *pszFilename = aosMetadata.FetchNameValue("FILENAME");
        if (pszFilename)
            info.pszFilename = CPLStrdup(pszFilename);
        info.nOffset = std::strtoull(
            aosMetadata.FetchNameValueDef("OFFSET", "0"), nullptr, 10);
        info.nSize = std::strtoull(aosMetadata.FetchNameValueDef("SIZE", "0"),
                                   nullptr, 10);
        const char *pszBase64 = aosMetadata.FetchNameValue("BASE64");
        if (pszBase64)
        {
            const size_t nSizeBase64 = strlen(pszBase64) + 1;
            info.pabyInlineData = static_cast<GByte *>(CPLMalloc(nSizeBase64));
            memcpy(info.pabyInlineData, pszBase64, nSizeBase64);
            const int nDecodedSize =
                CPLBase64DecodeInPlace(info.pabyInlineData);
            CPLAssert(static_cast<size_t>(nDecodedSize) ==
                      static_cast<size_t>(info.nSize));
            CPL_IGNORE_RET_VAL(nDecodedSize);
        }
    }
    else
    {
        info.pszFilename = CPLStrdup(osFilename.c_str());
        info.nOffset = 0;
        info.nSize = nFileSize;
    }

    info.papszInfo = CSLDuplicate(GetRawBlockInfoInfo().List());

    return true;
}

/************************************************************************/
/*                     ZarrArray::GetParentGroup()                      */
/************************************************************************/

std::shared_ptr<ZarrGroupBase> ZarrArray::GetParentGroup() const
{
    std::shared_ptr<ZarrGroupBase> poGroup = m_poParent.lock();
    if (!poGroup)
    {
        if (auto poRootGroup = m_poSharedResource->GetRootGroup())
        {
            const auto nPos = m_osFullName.rfind('/');
            if (nPos != 0 && nPos != std::string::npos)
            {
                poGroup = std::dynamic_pointer_cast<ZarrGroupBase>(
                    poRootGroup->OpenGroupFromFullname(
                        m_osFullName.substr(0, nPos)));
            }
        }
    }
    return poGroup;
}

/************************************************************************/
/*                    ZarrArray::IsRegularlySpaced()                    */
/************************************************************************/

// Process-level LRU cache for coordinate array regularity results.
// Keyed by root directory + array full name.
// Avoids redundant HTTP reads for immutable cloud-hosted coordinate arrays.
// Thread-safe: lru11::Cache with std::mutex handles locking internally.

struct CoordCacheEntry
{
    bool bIsRegular;
    double dfStart;
    double dfIncrement;
};

static lru11::Cache<std::string, CoordCacheEntry, std::mutex> g_oCoordCache{
    128};

void ZarrClearCoordinateCache()
{
    g_oCoordCache.clear();
}

bool ZarrArray::IsRegularlySpaced(double &dfStart, double &dfIncrement) const
{
    // Only cache 1D coordinate arrays (the ones that trigger HTTP reads)
    if (GetDimensionCount() != 1)
        return GDALMDArray::IsRegularlySpaced(dfStart, dfIncrement);

    const std::string &osKey = GetFilename();

    CoordCacheEntry entry;
    if (g_oCoordCache.tryGet(osKey, entry))
    {
        CPLDebugOnly("ZARR", "IsRegularlySpaced cache hit for %s",
                     osKey.c_str());
        dfStart = entry.dfStart;
        dfIncrement = entry.dfIncrement;
        return entry.bIsRegular;
    }

    // Cache miss: perform the full coordinate read
    const bool bResult = GDALMDArray::IsRegularlySpaced(dfStart, dfIncrement);

    g_oCoordCache.insert(osKey, {bResult, dfStart, dfIncrement});

    CPLDebugOnly("ZARR", "IsRegularlySpaced cached for %s: %s", osKey.c_str(),
                 bResult ? "regular" : "irregular");

    return bResult;
}
